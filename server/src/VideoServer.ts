/* eslint-disable @typescript-eslint/prefer-regexp-exec */
import {Logger} from "winston";
import {getLogger} from "../common/LoggerConfig";
import {WebServerProcess} from "../common/WebServerNode";
import http from "http";
import {loadConfig} from "../common/Config";
import {assertType} from "@ckitching/typescript-is";
import {Config, computeSegmentDuration, substituteManifestPattern} from "./Config";
import {ServerFileStore} from "./ServerFileStore";
import {StatusCodes as HttpStatus} from "http-status-codes";
import {assertNonNull} from "../common/util/Assertion";
import {ffmpeg} from "./Ffmpeg";
import {CamAPI} from "./CamAPI";

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
    constructor(nearEdgeIndex: number, nearEdge: string, initialIndex: number, initialTime: number = -1) {
        this.nearEdgeIndex = nearEdgeIndex;
        this.nearEdgePath = nearEdge;
        this.nearEdgeTime = Date.now();
        this.initialIndex = initialIndex;
        this.initialTime = (initialTime < 0) ? this.nearEdgeTime : initialTime;
    }

    /**
     * Returns the difference between when the near edge was uploaded, and when it was expected to be uploaded, in ms.
     *
     * Late gives positive numbers, and early gives negative numbers.
     */
    getNearEdgeDrift(segmentDuration: [number, number]): number {
        return this.calculateDrift(segmentDuration, this.nearEdgeTime, 0);
    }

    /**
     * Returns the difference between now, and when the far edge is expected to be uploaded, in ms.
     *
     * Late gives positive numbers, and early gives negative numbers.
     */
    getFarEdgeDrift(segmentDuration: [number, number]): number {
        return this.calculateDrift(segmentDuration,  Date.now(), 1);
    }

    /**
     * Determine whether the initial segment used for drift calculation is the final choice of initial segment.
     *
     * The first segment can arrive a bit late. So we let the second segment set the canonical start time. This could
     * actually wait for the third index if starting from zero, but that's OK.
     */
    isFinalInitial(): boolean {
        return this.initialIndex >= 2;
    }

    private calculateDrift(segmentDuration: [number, number], nowTime: number, segmentIncrement: number): number {
        const segmentsSinceStart = this.nearEdgeIndex + segmentIncrement - this.initialIndex;
        return nowTime - ((segmentsSinceStart * segmentDuration[0] * 1000 / segmentDuration[1]) + this.initialTime);
    }

    nearEdgeIndex: number;
    nearEdgePath: string;
    nearEdgeTime: number;

    initialIndex: number;
    initialTime: number;
}

class DriftInfo {
    add(drift: number): void {
        this.drifts.push(drift);
    }

    get() {
        let earliest: number = this.drifts[0];
        let latest: number = this.drifts[0];
        let sum = 0;
        for (const drift of this.drifts) {
            earliest = Math.min(earliest, drift);
            latest = Math.max(latest, drift);
            sum += drift;
        }
        return {
            earliest: earliest,
            latest: latest,
            mean: sum / this.drifts.length
        };
    }

    private readonly drifts = new Array<number>();
}

export class VideoServer extends WebServerProcess {
    private config!: Config;

    // The duration of each segment.
    private segmentDurationExact!: [number, number]; // In seconds.
    private segmentDuration!: number; // In milliseconds.

    // The set of files this server will respond to GET requests for. This is an in-memory cache.
    private readonly serverFileStore = new ServerFileStore();

    // Paths that are forbidden for client access.
    private readonly forbiddenPaths = new Array<RegExp>();

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

    private ffmpegProcesses: ffmpeg.Subprocess[] = [];

    // A unique string included in all URLs that prevents CDN caching of files generated by earlier runs of this
    // process.
    private readonly uniqueID: string;

    constructor() {
        super("VideoServer");
        this.uniqueID = Math.round(Date.now() + Math.random() * 1000000).toString(36);
    }

    private loadCacheConfiguration() {
        /* Add the caching times. Note that near edge paths are by definition guaranteed to be found. Far edge paths are
           known about and their cache time is specially calculated. */
        this.foundCacheTimes.set(Liveness.none, this.config.network.nonLiveCacheTime);
        this.foundCacheTimes.set(Liveness.live, Math.round(this.segmentDuration * (this.config.network.segmentRetentionCount + 1) / 1000));
        this.foundCacheTimes.set(Liveness.nearEdge, this.foundCacheTimes.get(Liveness.live)!);
        this.foundCacheTimes.set(Liveness.farEdge, this.foundCacheTimes.get(Liveness.live)! + Math.round(this.config.network.preAvailabilityTime / 1000));
        this.foundCacheTimes.set(Liveness.ephemeral, 1);

        this.notFoundCacheTimes.set(Liveness.none, this.config.network.nonLiveCacheTime);
        this.notFoundCacheTimes.set(Liveness.ephemeral, 1); // Shortest possible caching that isn't disabled.

        // Calculate live 404 time under the assumption that the requested path is the next far live edge. Round up so
        // we don't get a barrage of uncacheable requests near pre-availability time.
        this.notFoundCacheTimes.set(Liveness.live, Math.max(Math.round((this.segmentDuration - this.config.network.preAvailabilityTime) / 1000), 1));

        if (this.notFoundCacheTimes.get(Liveness.live) == 0) {
            log.warn('Warning: Caching of 404s for paths that may become live is disabled. Consider increasing pre-availability time.');
        }

        log.debug("Cache times:");
        this.foundCacheTimes.forEach((value: number, key: Liveness): void => {
            log.debug(`  200 ${Liveness[key]}: ${value}`);
        });
        this.notFoundCacheTimes.forEach((value: number, key: Liveness): void => {
            log.debug(`  404 ${Liveness[key]}: ${value}`);
        });

        const manifestPattern = substituteManifestPattern(this.config.dash.manifest, this.uniqueID);
        const livePattern = manifestPattern.replace(/[^/]*$/, ''); // Manifest path up to and including the last '/'.
        const edgePattern = '(?<=' + livePattern + '.*-)[0-9]+(?=(?:[.][^.]+)?$)'; // The end matches '-012345.xyz' or '-012345'.

        this.livePaths = new RegExp(livePattern);
        this.edgePaths = new RegExp(edgePattern);
        this.ephemeralPaths = new RegExp('^(?:(?:' + manifestPattern + ')|(?:' + this.config.serverInfo.live + ')|(?:' + livePattern + 'drift.json))$');
    }

    async start(config?: Config): Promise<void> {
        this.config = config ?? loadConfig('video-server');
        assertType<Config>(this.config);

        /* Calculate the segment duration. */
        this.segmentDurationExact = computeSegmentDuration(this.config.dash.segmentLengthMultiplier, this.config.video.configs);
        this.segmentDuration = (this.segmentDurationExact[0] * 1000) / this.segmentDurationExact[1];

        this.serverFileStore.on('add', (path: string): void => {
            this.onFileStoreAdd(path);
        });

        this.serverFileStore.on('remove', (path: string): void => {
            this.onFileStoreRemove(path);
        });

        await super.start();
    }

    protected registerStartJobs() {
        super.registerStartJobs();

        this.registerStartupJob("Write server info", async() => {
            this.loadCacheConfiguration();
            this.writeServerInfo();
            this.addInterleavePatterns();
        });

        // Aggressively reconfigure the cameras on every startup in production.
        if (this.config.camera.configure) {
            this.registerStartupJob(`Configure cameras`, async() => {
                await Promise.all(this.config.video.sources.map(async(src, i) => this.configureCamera(src, i)));
            }, {before: "Launch webserver"});
        }

        if (this.config.camera.powerOn) {
            this.registerStartupJob(`Turn cameras on`, async() => {
                await Promise.all(this.config.video.sources.map(async(src) => {
                    return (await this.getCamAPI(src)).turnOn();
                }));
            });
        }

        this.registerStartupJob("Begin streaming", async() => {
            await this.startStreaming();
        });
    }

    registerStopJobs() {
        super.registerStopJobs();

        this.registerShutdownJob("Stop ffmpeg processes", async() => {
            await this.stopStreaming();
        }, {before: "Stop HTTP server"});

        if (this.config.camera.powerOff) {
            this.registerShutdownJob(`Turn cameras off`, async() => {
                await Promise.all(this.config.video.sources.map(async(src) => {
                    return (await this.getCamAPI(src)).turnOff();
                }));
            });
        }
    }

    private async getCamAPI(srcUrl: string) {
        const url = "http://" + srcUrl.slice(7, srcUrl.lastIndexOf(':'));
        const api = new CamAPI(url, this.config.camera);
        await api.authenticate();
        return api;
    }

    // Apply the configuration we want to the camera using its undocumented REST API :D. This also power-cycles the
    // camera, recovering it from any broken state.
    private async configureCamera(srcUrl: string, num: number) {
        const api = await this.getCamAPI(srcUrl);
        const camNum = num + 1;

        log.info(`Cam ${camNum}: Checking camera firmware compatibility...`);
        await api.checkFirmwareVersion();

        // Power cycle the camera, since occasionally it gets stuck :D.
        if (this.config.camera.configurePowerCycle) {
            log.info(`Cam ${camNum}: Power off...`);
            await api.turnOff();

            log.info(`Cam ${camNum}: Power on...`);
            await api.turnOn();
        }

        log.info(`Cam ${camNum}: Setting stream configuration...`);
        await api.configureStreams(`Cam ${num + 1}`);

        log.info(`Cam ${camNum}: Setting audio configuration...`);
        await api.configureAudio(num == 0);

        log.info(`Cam ${camNum}: Setting clock configuration...`);
        await api.configureClock();

        // Power cycle the camera, since occasionally it gets stuck :D.
        if (this.config.camera.configurePowerCycle) {
            log.info(`Cam ${camNum}: Power off...`);
            await api.turnOff();

            log.info(`Cam ${camNum}: Power on...`);
            await api.turnOn();
        }
    }

    /**
     * Returns a map from video quality to the corresponding interleaved audio quality.
     *
     * Video qualities without audio are not included in the map.
     */
    private getAudioVideoMap(): [number, number][] {
        if (!this.config.dash.interleave) {
            return [];
        }

        const videoConfigCount = this.config.video.configs.length;
        const audioConfigCount = this.config.audio.configs.length;

        const result = new Array<[number, number]>();
        for (let i = 0; i < videoConfigCount && i < audioConfigCount; i++) {
            result.push([i, i + videoConfigCount]);
        }
        return result;
    }

    /**
     * Write some configuration metadata to the virtual filesystem, so clients can ask how many cameras we have.
     */
    private writeServerInfo() {
        const manifests = new Array<any>();
        this.config.video.sources.forEach((_: string, index: number): void => {
            manifests.push({
                name: 'Angle ' + index,
                path: substituteManifestPattern(this.config.dash.manifest, this.uniqueID, index)
            });
        });

        const sf = this.serverFileStore.add(this.config.serverInfo.live);
        sf.add(Buffer.from(JSON.stringify({
            angles: manifests,
            segmentDuration: this.segmentDuration,
            segmentPreavailability: this.config.network.preAvailabilityTime,
            avMap: this.getAudioVideoMap()
        })));

        sf.finish();
    }

    /**
     * Set up file interleaving.
     */
    private addInterleavePatterns(): void {
        const avMap = this.getAudioVideoMap();

        for (let index = 0; index < this.config.video.sources.length; index++) {
            // Manifest path up to and including the last '/'.
            const livePrefix = substituteManifestPattern(this.config.dash.manifest, this.uniqueID, index).replace(/[^/]*$/, '');

            for (const va of avMap) {
                const videoRegex = new RegExp(`${livePrefix}chunk-stream${va[0]}-([0-9]+).[^.]+`);
                const audioRegex = new RegExp(`${livePrefix}chunk-stream${va[1]}-([0-9]+).[^.]+`);
                this.serverFileStore.addInterleavingPattern(`${livePrefix}interleaved${va[0]}-{1}`, [videoRegex, audioRegex]);

                if (!this.config.dash.interleavedDirectDashSegments) {
                    this.forbiddenPaths.push(videoRegex);
                    this.forbiddenPaths.push(audioRegex);
                }
            }
        }
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
            let farEdgeDrift = 0;
            if (this.nearEdgePaths.has(path)) {
                liveness = Liveness.nearEdge;
            } else if (this.farEdgePaths.has(path)) {
                // The far edge here is any path that is the immediate successor of a near-edge path, even if it's not
                // pre-available yet.
                liveness = Liveness.farEdge;
                const timeSinceNearEdgeAdded = now - this.farEdgePaths.get(path)!.nearEdgeTime;
                const preAvailabilityDelay = this.segmentDuration - this.config.network.preAvailabilityTime;
                timeToFarEdgePreavailability = preAvailabilityDelay - timeSinceNearEdgeAdded;
                if (request.method == 'PUT') {
                    farEdgeDrift = this.farEdgePaths.get(path)!.getFarEdgeDrift(this.segmentDurationExact);
                    if (this.farEdgePaths.get(path)!.isFinalInitial() && this.config.dash.terminateDriftLimit != 0 && Math.abs(farEdgeDrift) > this.config.dash.terminateDriftLimit) {
                        this.fatalError(`Live edge drifted too far (${farEdgeDrift} ms)!`);
                    }
                }
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
                if (request.method == 'PUT') {
                    requestInfoString += ` (drift: ${farEdgeDrift} ms)`;
                }
            }
            log.debug(new Date(now).toISOString() + ' - Received request: ' + requestInfoString);

            request.on('error', (e): void => {
                log.error('Error in request `$s`: %O', requestInfoString, e);
            });
            response.on('error', (e): void => {
                log.error('Error in response `$s`: %O', requestInfoString, e);
            });

            /* Configure for low latency. */
            assertNonNull(response.socket);
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

        /* See whether the request is for a banned URL. */
        for (const regex of this.forbiddenPaths) {
            if (!regex.test(request.url)) {
                continue;
            }
            response.statusCode = HttpStatus.FORBIDDEN;
            this.addCacheControl(response, Liveness.none, false);
            response.end();
            return;
        }

        /* Try to find the requested path. */
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
        if (this.serverFileStore.has(request.url)) {
            this.serverFileStore.remove(request.url);
            response.statusCode = HttpStatus.OK;
        } else {
            response.statusCode = HttpStatus.NOT_FOUND;
        }
        response.end();
    }

    private onFileStoreAdd(path: string): void {
        // Handle edge paths.
        let farEdgeInfo: EdgeInfo | null = null;
        if (this.farEdgePaths.has(path)) {
            farEdgeInfo = this.farEdgePaths.get(path)!;

            // Whatever else can be said, this path is no longer a far-edge. The predecessor for this path is no longer
            // the near edge either.
            this.nearEdgePaths.delete(this.farEdgePaths.get(path)!.nearEdgePath);
            this.farEdgePaths.delete(path);
        }

        const edgeMatch = path.match(this.edgePaths);

        if (edgeMatch?.index !== undefined) {
            // Figure out the next path.
            const prefix = path.substr(0, edgeMatch.index);
            const indexString = edgeMatch[0];
            const suffix = path.substr(edgeMatch.index + indexString.length);

            const index = parseInt(indexString);
            let nextIndexString = '' + (index + 1);
            if (nextIndexString.length < indexString.length) {
                nextIndexString = nextIndexString.padStart(indexString.length, '0');
            } else if (nextIndexString.length > indexString.length) {
                nextIndexString = nextIndexString.substr(nextIndexString.length - indexString.length, indexString.length);
            }
            const nextPath = prefix + nextIndexString + suffix;

            // Add the next path as a far edge once this segment has existed for long enough.
            let edgeInfo: EdgeInfo;
            if (farEdgeInfo) {
                if (!farEdgeInfo.isFinalInitial()) {
                    edgeInfo = new EdgeInfo(index, path, index); // Not final initial (i.e: reference) segment yet.
                } else {
                    edgeInfo = new EdgeInfo(index, path, farEdgeInfo.initialIndex, farEdgeInfo.initialTime);
                }
            } else {
                if (index != 0 && index != 1) {
                    this.fatalError(`Live-edge path ${path} added that is neither a starting index nor from the far edge!`);
                }
                edgeInfo = new EdgeInfo(index, path, index);
            }
            this.farEdgePaths.set(nextPath, edgeInfo);

            // Add this path as a near edge.
            this.nearEdgePaths.set(path, edgeInfo.nearEdgeTime);

            // Update the drift file.
            this.updateDriftFiles();
        }
    }

    private onFileStoreRemove(path: string): void {
        this.nearEdgePaths.delete(path);
        this.farEdgePaths.delete(path);
    }

    private updateDriftFiles(): void {
        const driftMap = new Map<string, DriftInfo>();
        this.farEdgePaths.forEach((edgeInfo: EdgeInfo): void => {
            const driftPath = edgeInfo.nearEdgePath.replace(/[^/]+$/, 'drift.json');
            if (!driftMap.has(driftPath)) {
                driftMap.set(driftPath, new DriftInfo());
            }
            driftMap.get(driftPath)!.add(edgeInfo.getNearEdgeDrift(this.segmentDurationExact));
        });

        driftMap.forEach((driftInfo: DriftInfo, driftPath: string): void => {
            const sf = this.serverFileStore.add(driftPath);
            sf.add(Buffer.from(JSON.stringify(driftInfo.get())));
            sf.finish();
        });
    }

    private fatalError(msg: string): void {
        log.error(msg);
        void (this.stop()); // I don't see what else I'd do with this promise.
    }

    public async startStreaming() {
        log.info("Building ffmpeg processes...");
        this.ffmpegProcesses = this.config.video.sources.map((source: string, i: number) => {
            return ffmpeg.launchTranscoder(i, this.config, source, this.uniqueID);
        });

        log.info("Launching ffmpeg processes...");
        await Promise.all(this.ffmpegProcesses.map(async(p) => p.start()));
        log.info("All ffmpeg processes started.");
    }

    public async stopStreaming() {
        log.info("Shutting down ffmpeg processes...");
        await Promise.all(this.ffmpegProcesses.map(async(p) => p.stop(10000)));
        log.info("All ffmepg processes have stopped.");
    }
}
