declare const manifestUrl: string;

/* Things computed upon the above. */
const manifestUrlPrefix = manifestUrl.replace(/[^/]*$/, '');

/* Manifest parsing. */
class XmlParser {
    constructor(text: string) {
        /* Parse the XML. */
        // There must be a better way to handle the namespace than just... removing it. The createNSResolver
        // below should work.
        const filteredText = text.replace(/(xmlns[^=]*|xsi:[^=]*)="[^"]*"/g, '');
        this.xml = new DOMParser().parseFromString(filteredText, 'text/xml');

        /* A namespace resolver. */
        // XML is special sometimes. This is needed even with the namespace stripping regex.
        this.ns = this.xml.createNSResolver(this.xml.documentElement);
    }

    getString(path: string, context: Element = null): string {
        return this.xml.evaluate(path, (context === null) ? this.xml : context, this.ns,
                                 XPathResult.STRING_TYPE).stringValue;
    }

    getNumber(path: string, context: Element = null): number {
        return parseFloat(this.getString(path, context));
    }

    getElement(path: string, context: Element = null) {
        return this.xml.evaluate(path, (context === null) ? this.xml : context, this.ns,
                                 XPathResult.FIRST_ORDERED_NODE_TYPE).singleNodeValue;
    }

    getElements(path: string, context: Element = null) {
        let result = new Array();
        let snapshot = this.xml.evaluate(path, (context === null) ? this.xml : context, this.ns,
                                         XPathResult.ORDERED_NODE_SNAPSHOT_TYPE);
        for (let i = 0; i < snapshot.snapshotLength; i++) {
            result.push(snapshot.snapshotItem(i));
        }
        return result;
    }

    getStrings(path: string, context: Element = null): Array<string> {
        let result = new Array<string>();
        const elements = this.getElements(path, context);
        for (let i = 0; i < elements.length; i++) {
            result.push(elements[i].value);
        }
        return result;
    }

    getNumbers(path: string, context: Element = null): Array<number> {
        let result = new Array<number>();
        const strings = this.getStrings(path, context);
        for (let i = 0; i < strings.length; i++) {
            result.push(parseFloat(strings[i]));
        }
        return result;
    }

    private xml: XMLDocument;
    private ns: XPathNSResolver;
};

export class Representation {
    constructor(id: number, duration: number, availabilityTimeOffset: number, init: string, media: string,
                bandwidth: number, mimeType: string, codec: string) {
        const initUrl = manifestUrlPrefix + init.replace('$RepresentationID$', '' + id);
        this.mediaUrlPattern = manifestUrlPrefix + media.replace('$RepresentationID$', '' + id);
        this.duration = duration;
        this.startDelay = duration - availabilityTimeOffset; // We count from the start, not the end.
        this.bandwidth = bandwidth;
        this.mimeType = mimeType + '; codecs="' + codec + '"';

        console.log('Representation:\n' +
                    '  Segment duration: ' + this.duration + '\n' +
                    '  Segment start delay: ' + this.startDelay + '\n' +
                    '  Initialization: ' + initUrl + '\n' +
                    '  Segment pattern: ' + this.mediaUrlPattern + '\n' +
                    '  Bandwidth: ' + this.bandwidth + ' bit/s\n' +
                    '  Bandwidth: ' + this.mimeType);

        // Prevetch the initialization segments for fast switching. This is actually only a few kilobytes.
        this.initData = fetch(initUrl).then((response: Response) => {
            return response.arrayBuffer();
        });
    }

    getSegmentUrl(index: number): string {
        // Rather crude, but works in our use case.
        let result = this.mediaUrlPattern;
        for (let n = 1; n < 10; n++) {
            let indexString = '' + index;
            if (indexString.length < n) {
                indexString = indexString.padStart(n, '0');
            }
            else {
                indexString = indexString.substr(0, n);
            }
            result.replace('$Number%0' + n + 'd$', indexString);
        }
        return result;
    }

    mediaUrlPattern: string;
    duration: number;
    startDelay: number;
    bandwidth: number;
    mimeType: string;
    initData: Promise<ArrayBuffer>;
};

export class Manifest {
    constructor(text: string) {
        const parseDuration = function (s: string): number {
            // We never use anything that would go beyond seconds.
            return parseFloat(s.match(/^PT([0-9.]+)S$/)[1]) * 1000;
        };

        /* Turn the XML text into something vaguely useable. */
        const xml = new XmlParser(text);

        /* Extract information about the whole MPD. */
        this.startTime = new Date(xml.getString('/MPD/@availabilityStartTime')).valueOf();
        this.timeSyncUrl = xml.getString('/MPD/UTCTiming/@value');

        // And print it :)
        console.log('Start time: ' + this.startTime + '\n' +
                    'Time sync URL: ' + this.timeSyncUrl + '\n');

        /* Extract information about each presentation. */
        const getAdaptationSetRepresentations = (type: string): Array<Representation> => {
            // We really only expect one adaptation set of each type.
            const ids = xml.getNumbers('/MPD/Period/AdaptationSet[@contentType="' + type + '"]/Representation/@id');
            const bandwidths = xml.getNumbers('/MPD/Period/AdaptationSet[@contentType="' + type + '"]/Representation/@bandwidth');
            const mimeTypes = xml.getStrings('/MPD/Period/AdaptationSet[@contentType="' + type + '"]/Representation/@mimeType');
            const codecs = xml.getStrings('/MPD/Period/AdaptationSet[@contentType="' + type + '"]/Representation/@codecs');
            const inits = xml.getStrings('/MPD/Period/AdaptationSet[@contentType="' + type + '"]/Representation/SegmentTemplate/@initialization');
            const medias = xml.getStrings('/MPD/Period/AdaptationSet[@contentType="' + type + '"]/Representation/SegmentTemplate/@media');
            const durations = xml.getNumbers('/MPD/Period/AdaptationSet[@contentType="' + type + '"]/Representation/SegmentTemplate/@duration');
            const availabilityTimeOffsets = xml.getNumbers('/MPD/Period/AdaptationSet[@contentType="' + type + '"]/Representation/SegmentTemplate/@availabilityTimeOffset');

            const representations = new Array<Representation>();
            for (let i = 0; i < ids.length; i++) {
                representations.push(new Representation(ids[i], durations[i], availabilityTimeOffsets[i],
                                                        inits[i], medias[i], bandwidths[i], mimeTypes[i], codecs[i]));
            }
            return representations;
        };

        this.videoRepresentations = getAdaptationSetRepresentations('video');
        this.audioRepresentations = getAdaptationSetRepresentations('audio');
    }

    startTime: number;
    timeSyncUrl: string;
    videoRepresentations: Array<Representation>;
    audioRepresentations: Array<Representation>;
};
