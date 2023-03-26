import * as chart from "chart.js";
import {BufferControl, BufferControlTickInfo, ReceivedInfo, TimestampInfo} from "live-video-streamer-client";

chart.Chart.register(...chart.registerables);

chart.Chart.defaults.borderColor = "rgb(32, 32, 32)"; // TODO maybe not the best idea...
chart.Chart.defaults.color = "rgb(191, 191, 191)";

/**
 * Class for showing information for debugging performance/etc.
 */
export class AppDebugHandler {
    constructor() {
        /* Common configuration between timelines. */
        const timelineConfig = {
            type: "line" as const,
            data: {
                datasets: [
                    {
                        label: "Catch Up Event",
                        backgroundColor: "rgb(255, 0, 0)",
                        borderColor: "rgb(255, 0, 0)",
                        data: [],

                        type: "scatter" as const,
                        pointRadius: 4
                    }, {
                        label: "Initial Seek Event",
                        backgroundColor: "rgb(123,0,127)",
                        borderColor: "rgb(95,0,127)",
                        data: [],

                        type: "scatter" as const,
                        pointRadius: 4
                    }, {
                        label: "Segment Start",
                        backgroundColor: "rgb(127, 127, 127)",
                        borderColor: "rgb(127, 127, 127)",
                        data: [],

                        type: "scatter" as const,
                        pointRadius: 2
                    }
                ]
            },
            options: {
                animation: false as const,
                aspectRatio: 6,
                elements: {
                    line: {
                        borderWidth: 1
                    },
                    point: {
                        radius: 0
                    }
                },
                scales: {
                    x: {
                        type: "linear" as const,
                        position: "bottom" as const,
                        ticks: {
                            format: {minimumFractionDigits: 1, maximumFractionDigits: 1},
                            maxTicksLimit: 25
                        }
                    },
                    y: {
                        type: "linear" as const,
                        position: "left" as const,
                        beginAtZero: true
                    }
                }
            }
        };
        const copyTimelineConfig = (): typeof timelineConfig => {
            // Javascript needs a better deep copy.
            return JSON.parse(JSON.stringify(timelineConfig)) as typeof timelineConfig;
        };

        /* Function to append a dataset to a timeline chart. */
        const appendTimelineDataset = (c: chart.Chart, label: string, colour: string): void => {
            c.data.datasets.splice(c.data.datasets.length - copyTimelineConfig().data.datasets.length, 0, {
                label: label,
                backgroundColor: colour,
                borderColor: colour,
                data: []
            });
        };

        /* Latency chart. */
        this.latencyChart = new chart.Chart(document.getElementById("latency_timeline")! as HTMLCanvasElement,
                                            copyTimelineConfig());
        appendTimelineDataset(this.latencyChart, "Target Buffer", "rgb(255, 127, 0)");
        appendTimelineDataset(this.latencyChart, "Actual Buffer", "rgb(0, 255, 0)");
        appendTimelineDataset(this.latencyChart, "Network Latency", "rgb(255,233,64)");
        this.latencyChart.update();

        /* Download rate chart. */
        this.dlChart = new chart.Chart(document.getElementById("dlrate_timeline")! as HTMLCanvasElement,
                                       copyTimelineConfig());
        appendTimelineDataset(this.dlChart, "Download Rate", "rgb(127, 127, 255)");
        appendTimelineDataset(this.dlChart, "Average Download Rate", "rgb(0,127,106)");
        this.dlChart.update();

        /* Offset all timestamps by this amount. */
        this.start = Date.now();
    }

    setBufferControl(bctrl: BufferControl): void {
        this.bctrl = bctrl;
    }

    onBufferControlTick(tickInfo: BufferControlTickInfo): void {
        /* Calculate the timestamp. */
        const t = (tickInfo.timestamp - this.start) / 1000;

        /* Add the various data points. */
        if (tickInfo.catchUp) {
            for (const c of [this.latencyChart, this.dlChart]) {
                this.getData(c, "Catch Up Event").push({x: t, y: 0});
            }
        }
        if (tickInfo.initialSeek) {
            for (const c of [this.latencyChart, this.dlChart]) {
                this.getData(c, "Initial Seek Event").push({x: t, y: 0});
            }
        }
        this.getData(this.latencyChart, "Target Buffer").push({x: t, y: this.bctrl.getBufferTarget()});
        this.getData(this.latencyChart, "Actual Buffer").push({x: t, y: tickInfo.primaryBufferLength});

        /* We have a new timeline data point. */
        this.updateTimelines(tickInfo.timestamp);
    }

    onTimestamp(timestampInfo: TimestampInfo): void {
        const t = (timestampInfo.sentTimestamp - this.start) / 1000;
        const d = timestampInfo.endReceivedTimestamp - timestampInfo.sentTimestamp;
        this.getData(this.latencyChart, "Network Latency").push({x: t, y: d});
        if (timestampInfo.firstForInterleave) {
            for (const c of [this.latencyChart, this.dlChart]) {
                this.getData(c, "Segment Start").push({x: t, y: 0});
            }
        }
        this.updateTimelines(t);
    }

    onReceived(receivedInfo: ReceivedInfo): void {
        /* Accumulate the received data. But not the first few seconds. That data isn't meaningful. */
        if (receivedInfo.timestamp - this.start >= this.dlRatePlotStart) {
            this.accumulatedData += receivedInfo.length;
            this.avgDlRateHistory.push(receivedInfo);
        }

        /* Don't plot too often. */
        if (this.lastDlRateTimestamp === 0) {
            this.lastDlRateTimestamp = receivedInfo.timestamp;
        }
        if (receivedInfo.timestamp - this.lastDlRateTimestamp < this.dlRatePlotRate) {
            return;
        }

        /* Calculate the download rate in bytes/s. */
        const rate: number = this.accumulatedData * 1000 / (receivedInfo.timestamp - this.lastDlRateTimestamp);

        /* The time of the last download rate data point has changed. */
        this.lastDlRateTimestamp = receivedInfo.timestamp;
        this.accumulatedData = 0;

        /* Don't plot in the first few seconds, as the initial values aren't meaningful. */
        if (receivedInfo.timestamp - this.start < this.dlRatePlotStart) {
            return;
        }

        /* Plot the rate in kBit/s. */
        this.getData(this.dlChart, "Download Rate").push({x: (receivedInfo.timestamp - this.start) / 1000,
                                                          y: rate / 128});

        /* Prune the history to the last dlAvgRateHistoryLength ms. */
        const sliceStart: number = this.avgDlRateHistory.findIndex((e: ReceivedInfo): boolean => {
            // Keep those new enough.
            return e.timestamp >=
                   this.avgDlRateHistory[this.avgDlRateHistory.length - 1]!.timestamp - this.dlAvgRateHistoryLength;
        });
        this.avgDlRateHistory = this.avgDlRateHistory.slice(sliceStart);
        if (this.avgDlRateHistory.length < 2) {
            return; // We need at least two points.
        }

        /* Figure out the total data transferred in the history. This skips the first element's data because its
           timestamp is the end of the download. */
        const totalData = this.avgDlRateHistory.slice(1).map((e: ReceivedInfo): number => e.length)
            .reduce((a: number, b: number): number => a + b);

        /* Time window over which the total data was received. */
        const timeWindow = this.avgDlRateHistory[this.avgDlRateHistory.length - 1]!.timestamp -
                           this.avgDlRateHistory[0]!.timestamp;

        /* Calculate the average data rate in bytes per second. */
        const avgRate: number = totalData * 1000 / timeWindow;

        /* Plot it in kBit/s. */
        this.getData(this.dlChart, "Average Download Rate").push({x: (receivedInfo.timestamp - this.start) / 1000,
                                                                  y: avgRate / 128});
    }

    /**
     * Get the data array for the given chart/label pair.
     *
     * @param c The chart to get the data array from.
     * @param label The dataset label to get the data array for.
     * @return The data array.
     */
    private getData(c: chart.Chart, label: string): unknown[] {
        for (const dataset of c.data.datasets) {
            if (dataset.label === label) {
                return dataset.data;
            }
        }
        throw Error(`Bad label ${label} for chart.`);
    }

    /**
     * Update the time axes on all the timeline charts.
     *
     * @param time The time of the most recent data point.
     */
    private updateTimelines(time: number): void {
        /* Figure out the start and end times. */
        this.timelineLatest = Math.max(this.timelineLatest, time - this.start);
        const max = this.timelineLatest / 1000;
        const min = max - 120;

        /* Update the charts' data. */
        for (const c of [this.latencyChart, this.dlChart]) {
            // Apply the start and end to the timeline chart.
            c.options.scales!["x"]!.min = min;
            c.options.scales!["x"]!.max = max;

            // Prune the chart.
            const maxLength = 1500;
            for (const dataset of c.data.datasets) {
                if (dataset.data.length > maxLength + 100) {
                    dataset.data = dataset.data.slice(dataset.data.length - maxLength, dataset.data.length);
                }
            }
        }

        /* Update all the charts at once. */
        for (const c of [this.latencyChart, this.dlChart]) {
            c.update();
        }
    }

    private bctrl!: BufferControl;

    private readonly start: number; // Timestamp of what we're calling time 0.
    private timelineLatest: number = 0; // The time of the latest data point.

    private readonly dlRatePlotStart: number = 2000; // Wait this long, in ms, before plotting download rate.
    private readonly dlRatePlotRate: number = 100; // Time, in ms, between download rate plots.
    private lastDlRateTimestamp: number = 0; // Timestamp of the last download rate data point that was plotted.
    private accumulatedData: number = 0; // Bytes of data received since the last download rate data point was plotted.
    private readonly dlAvgRateHistoryLength: number = 10000; // History length for average data rate plot, in ms.
    private avgDlRateHistory: ReceivedInfo[] = []; // Last dlAvgRateHistoryLength ms of data for plotting average rate.

    private readonly latencyChart: chart.Chart; // Chart to show the latency timeline.
    private readonly dlChart: chart.Chart; // Chart to show the download rate timeline.
}
