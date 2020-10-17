/* eslint-disable @typescript-eslint/prefer-regexp-exec */
import {Logger} from "winston";
import {getLogger} from "../common/LoggerConfig";
import {ZmqEndpoint, ZmqTarget} from "../common/network/ZmqLayer";
import {WebServerNode} from "../common/WebServerNode";
import http from "http";
import {loadConfig} from "../common/Config";
import {assertType} from "@ckitching/typescript-is";
import {Config} from "./Config";
import {ServerFileStore} from "./ServerFileStore";
import {StatusCodes as HttpStatus} from "http-status-codes";
import {assertNonNull} from "../common/util/Assertion";
import {ffmpeg} from "./Ffmpeg";

export const log: Logger = getLogger("VideoServer");

enum Liveness {
    // Nothing to do with the live stream.
    none,

    // Persistent-ish live-stream data (e.g: segments).
    live,

    // Data that's at the near-edge (e.g: the existing live-edge segment).
    nearEdge,

    // Data that's at the far edge (e.g: a future segment if it's close enough to its start time).
    farEdge,

    // Data that changes often, e.g: whenever the edge segment changes.
    ephemeral
}

class EdgeInfo {
    constructor(nearEdge: string) {
        this.nearEdgePath = nearEdge;
        this.nearEdgeTime = Date.now();
    }

    nearEdgePath: string;
    nearEdgeTime: number;
}

function substituteManifestPattern(pattern: string, index?: number): string {
    let result = pattern;

    // eslint-disable-next-line no-constant-condition
    while (true) {
        const match = result.match('[{]([0-9]+)[}]');
        if (match == null) {
            break;
        }

        assertNonNull(match);
        assertNonNull(match.index);

        const startIdx = match.index;
        const endIdx = match.index + match[0].length;
        const numDigits = parseInt(match[1]);

        const replacement = (index === undefined) ?
            ('[0-9]{' + numDigits + ',}') : index.toString().padStart(numDigits, '0');
        result = result.substr(0, startIdx) + replacement + result.substr(endIdx);
    }
    return result;
}

export class VideoServer extends WebServerNode {
    private config!: Config;

    // The set of files this server will respond to GET requests for. This is an in-memory cache.
    private readonly serverFileStore = new ServerFileStore();

    // How long should the CDN cache responses to GET requests that were satisfied by the `serverFileStore`, for each liveness level.
    private readonly foundCacheTimes = new Map<Liveness, number>();

    // How long should the CDN cache 404 responses for, for each liveness level?
    private readonly notFoundCacheTimes = new Map<Liveness, number>();

    // Keeps information about when ffmpeg started uploading a file that it is currently still uploading, and what the next file in the sequence is.
    private readonly nearEdgePaths = new Map<string, number>();

    // Keeps information about what ffmpeg is expected to upload next (when it's expected to start, and what file it follows).
    private readonly farEdgePaths = new Map<string, EdgeInfo>();

    // Which paths should be subject to the special "HTTP 200 then stall" behaviour?
    // When a GET request for a path matching thisn regex hits a 404 condition, the server instead responds HTTP 200 and
    // then waits for the requested file to start existing. When it does, the server serves it. This allows the client
    // to request portions of the video that don't yet exist, but will start existing very soon, reducing how often they
    // back off.
    private edgePaths!: RegExp;

    // Regexes for classifying request paths into liveness categories.
    private livePaths!: RegExp;
    private ephemeralPaths!: RegExp;

    constructor() {
        super("VideoServer", ZmqEndpoint.VIDEO_SERVER, ZmqEndpoint.NONE, ZmqTarget.NONE);
    }

    async start(config?: Config): Promise<void> {
        this.config = config ?? loadConfig('video-server');
        assertType<Config>(this.config);

        /* Add the caching times. Note that near edge paths are by definition guaranteed to be found. Far edge paths are
           known about and their cache time is specially calculated. */
        const segmentDurationInSeconds = this.config.dash.segmentDuration * 1000;
        this.foundCacheTimes.set(Liveness.none, this.config.network.nonLiveCacheTime);
        this.foundCacheTimes.set(Liveness.live, Math.round(segmentDurationInSeconds * (this.config.network.segmentRetentionCount + 1) / 1000));
        this.foundCacheTimes.set(Liveness.nearEdge, this.foundCacheTimes.get(Liveness.live)!);
        this.foundCacheTimes.set(Liveness.farEdge, this.foundCacheTimes.get(Liveness.live)! + Math.round(this.config.network.preAvailabilityTime / 1000));
        this.foundCacheTimes.set(Liveness.ephemeral, 1);

        this.notFoundCacheTimes.set(Liveness.none, this.config.network.nonLiveCacheTime);
        this.notFoundCacheTimes.set(Liveness.ephemeral, 1); // Shortest possible caching that isn't disabled.

        // Calculate live 404 time under the assumption that the requested path is the next far live edge. Round up so
        // we don't get a barrage of uncacheable requests near pre-availability time.
        this.notFoundCacheTimes.set(Liveness.live, Math.max(Math.round((segmentDurationInSeconds - this.config.network.preAvailabilityTime) / 1000), 1));

        if (this.notFoundCacheTimes.get(Liveness.live) == 0) {
            log.warning('Warning: Caching of 404s for paths that may become live is disabled. Consider increasing pre-availability time.');
        }

        log.debug("Cache times:");
        this.foundCacheTimes.forEach((value: number, key: Liveness): void => {
            log.debug(`  200 ${Liveness[key]}: ${value}`);
        });
        this.notFoundCacheTimes.forEach((value: number, key: Liveness): void => {
            log.debug(`  404 ${Liveness[key]}: ${value}`);
        });

        const manifestPattern = '^' + substituteManifestPattern(this.config.dash.manifest) + '$';
        const livePattern = manifestPattern.replace(/[^/]*$/, ''); // Manifest path up to and including the last '/'.
        const edgePattern = '(?<=' + livePattern + '.*-)[0-9]+(?=[.][^.]+$)'; // The end matches '-012345.xyz'.

        this.livePaths = new RegExp(livePattern);
        this.edgePaths = new RegExp(edgePattern);
        this.ephemeralPaths = new RegExp(manifestPattern);

        this.writeServerInfo();

        await super.start();

        this.startStreaming();
    }

    /**
     * Write some configuration metadata to the virtual filesystem, so clients can ask how many cameras we have.
     */
    private writeServerInfo() {
        const manifests = new Array<any>();
        this.config.video.sources.forEach((_: string, index: number): void => {
            manifests.push({
                name: 'Angle ' + index,
                path: substituteManifestPattern(this.config.dash.manifest, index)
            });
        });

        const sf = this.serverFileStore.add(this.config.serverInfo.live);
        sf.add(Buffer.from(JSON.stringify({
            angles: manifests,
            segmentDuration: this.config.dash.segmentDuration * 1000,
            segmentPreavailability: this.config.network.preAvailabilityTime
        })));

        sf.finish();
    }

    // Add the cache-control header appropriate for the liveness.
    private addCacheControl(response: http.ServerResponse, liveness: Liveness, found: boolean) {
        const maxAge = (found ? this.foundCacheTimes : this.notFoundCacheTimes).get(liveness);
        response.setHeader('Cache-Control', (maxAge == 0) ? 'no-cache' : ('public, max-age=' + maxAge));
    }

    private validateSecureRequest(address: string, response: http.ServerResponse): boolean {
        if (address == '127.0.0.1' || address == '::1') {
            return true;
        }

        response.statusCode = 403;
        this.addCacheControl(response, Liveness.none, false);
        response.end();
        return false;
    }

    protected buildServer(listenCallback: () => void): http.Server {
        return http.createServer((request: http.IncomingMessage, response: http.ServerResponse): void => {
            /* Extract request information. */
            const path = request.url;
            const address = request.socket.remoteAddress!;
            const port = request.socket.remotePort!;

            assertNonNull(path);
            assertNonNull(address);
            assertNonNull(port);

            /* Extract the liveness. */
            const now = Date.now();
            let liveness: Liveness = Liveness.none;
            let timeToFarEdgePreavailability = 0;
            if (this.nearEdgePaths.has(path)) {
                liveness = Liveness.nearEdge;
            } else if (this.farEdgePaths.has(path)) {
                // The far edge here is any path that is the immediate successor of a near-edge path, even if it's not
                // pre-available yet.
                liveness = Liveness.farEdge;
                const timeSinceNearEdgeAdded = now - this.farEdgePaths.get(path)!.nearEdgeTime;
                const preAvailabilityDelay = this.config.dash.segmentDuration - this.config.network.preAvailabilityTime;
                timeToFarEdgePreavailability = preAvailabilityDelay - timeSinceNearEdgeAdded;
            } else if (this.ephemeralPaths.test(path)) {
                liveness = Liveness.ephemeral;
            } else if (this.livePaths.test(path)) {
                liveness = Liveness.live;
            }

            /* Print information :) */
            let requestInfoString = `address: ${address}, port: ${port}, path: ${path}, method: ${request.method}; liveness: ${Liveness[liveness]}`;
            if (liveness == Liveness.nearEdge) {
                requestInfoString += ` (live for ${now - this.nearEdgePaths.get(path)!} ms)`;
            } else if (liveness == Liveness.farEdge) {
                requestInfoString += ` (pre-available in ${timeToFarEdgePreavailability} ms)`;
            }

            if (request.method == 'HEAD' || request.method == 'GET') {
                log.debug(new Date(now).toISOString() + ' - Received request: ' + requestInfoString);
            }

            request.on('error', (e): void => {
                log.error('Error in request `$s`: %O', requestInfoString, e);
            });
            response.on('error', (e): void => {
                log.error('Error in response `$s`: %O', requestInfoString, e);
            });

            /* Configure for low latency. */
            response.socket.setNoDelay();

            /* Switch on method. */
            switch (request.method) {
                case 'HEAD': {
                    this.handleGet(request, response, true, liveness, timeToFarEdgePreavailability);
                } break;

                case 'GET': {
                    this.handleGet(request, response, false, liveness, timeToFarEdgePreavailability);
                } break;

                case 'PUT': {
                    if (this.validateSecureRequest(address, response)) {
                        this.handlePut(request, response);
                    }
                } break;

                case 'DELETE': {
                    if (this.validateSecureRequest(address, response)) {
                        this.handleDelete(request, response);
                    }
                } break;

                default:
                    response.statusCode = HttpStatus.METHOD_NOT_ALLOWED;
                    this.addCacheControl(response, Liveness.none, false);
                    response.end();
            }
        }).listen(this.config.network.port, listenCallback);
    }

    // Request handlers.
    /////////////////////////////////////////////////

    private handleGet(request: http.IncomingMessage,
                      response: http.ServerResponse,
                      isHead: boolean,
                      liveness: Liveness,
                      timeToFarEdgePreavailability: number): void {
        assertNonNull(request.url);

        const doHead = () => {
            response.statusCode = HttpStatus.OK;

            this.addCacheControl(response, liveness, true);
            if (this.config.network.origin) {
                response.setHeader('Access-Control-Allow-Origin', this.config.network.origin);
            }

            response.setHeader('Transfer-Encoding', 'chunked'); // Node.js takes care of the rest of this.
            if (isHead) {
                response.end();
            }
        };

        const doTransfer = () => {
            const sf = this.serverFileStore.get(request.url!);
            sf.writeWith((b: Buffer): void => {
                response.write(b);
            });
            if (sf.isFinished()) {
                response.end();
                return;
            }

            sf.on('add', (b: Buffer) => {
                response.write(b);
            });
            sf.on('finish', () => {
                response.end();
            });
        };

        if (this.serverFileStore.has(request.url)) {
            doHead();
            if (!isHead) {
                doTransfer();
            }
            return;
        } else if (liveness == Liveness.farEdge && timeToFarEdgePreavailability > 0) {
            const maxAge = Math.ceil(timeToFarEdgePreavailability / 1000);
            response.statusCode = HttpStatus.NOT_FOUND;
            response.setHeader('Cache-Control', 'public, max-age=' + maxAge);
            response.end();
            return;
        } else if (liveness != Liveness.farEdge) {
            response.statusCode = HttpStatus.NOT_FOUND;
            this.addCacheControl(response, liveness, false);
            response.end();
            return;
        }

        /* Send the header now so that caching can happen. */
        doHead();
        if (isHead) {
            response.end();
            return;
        }

        /* When the file is added, be set up to serve it. */
        const onPut = (path: string): void => {
            /* Wait for the right path. */
            if (path != request.url) {
                return;
            }
            this.serverFileStore.off('add', onPut);

            /* If we got an intervening delete, then the best we can do is just finish now. This is really an error
               condition though. */
            if (!this.serverFileStore.has(request.url)) {
                log.warn(`Warning: ${request.url} was deleted while being served to active request!`);
                response.end();
                return;
            }

            /* Otherwise, set up the transfer. */
            doTransfer();
        };
        this.serverFileStore.on('add', onPut);
    }

    private handlePut(request: http.IncomingMessage, response: http.ServerResponse): void {
        assertNonNull(request.url);

        // Before we get here, we checked the request came from localhost. So we always accept.
        response.writeHead(200);

        // Handle edge paths.
        if (this.farEdgePaths.has(request.url)) {
            // Whatever else can be said, this path is no longer a far-edge. The predecessor for this path is no longer
            // the near edge either.
            this.nearEdgePaths.delete(this.farEdgePaths.get(request.url)!.nearEdgePath);
            this.farEdgePaths.delete(request.url);
        }

        const edgeMatch = request.url.match(this.edgePaths);

        if (edgeMatch?.index !== undefined) {
            // Figure out the next path.
            const prefix = request.url.substr(0, edgeMatch.index);
            const indexString = edgeMatch[0];
            const suffix = request.url.substr(edgeMatch.index + indexString.length);

            const index = parseInt(indexString);
            let nextIndexString = '' + (index + 1);
            if (nextIndexString.length < indexString.length) {
                nextIndexString = nextIndexString.padStart(indexString.length, '0');
            } else if (nextIndexString.length > indexString.length) {
                nextIndexString = nextIndexString.substr(nextIndexString.length - indexString.length,
                    indexString.length);
            }
            const nextPath = prefix + nextIndexString + suffix;

            // Add the next path as a far edge once this segment has existed for long enough.
            const edgeInfo = new EdgeInfo(request.url);
            this.farEdgePaths.set(nextPath, edgeInfo);

            // Add this path as a near edge.
            this.nearEdgePaths.set(request.url, edgeInfo.nearEdgeTime);
        }

        /* Create a server file, and make it accept this PUT's data. */
        const sf = this.serverFileStore.add(request.url);
        request.on('data', (b: Buffer): void => {
            sf.add(b);
        });
        request.on('end', (): void => {
            sf.finish();
            response.end();
        });
    }

    private handleDelete(request: http.IncomingMessage, response: http.ServerResponse): void {
        assertNonNull(request.url);

        if (!this.serverFileStore.has(request.url)) {
            response.statusCode = HttpStatus.NOT_FOUND;
            response.end();
            return;
        }

        this.serverFileStore.remove(request.url);
        this.nearEdgePaths.delete(request.url);
        this.farEdgePaths.delete(request.url);

        response.statusCode = HttpStatus.OK;
        response.end();
    }

    public startStreaming() {
        log.info("Launching ffmpeg processes...");
        this.config.video.sources.map((source: string) =>
            ffmpeg.launchTranscoder(true, this.config, source)
        );
    }

    public stopStreaming() {
        // TODO
    }
}
