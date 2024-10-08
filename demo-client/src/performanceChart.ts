import * as chart from "chart.js";
import {BufferControl, BufferControlTickInfo, ReceivedInfo, TimestampInfo} from "live-video-streamer-client";

chart.Chart.register(...chart.registerables);

chart.Chart.defaults.borderColor = "rgb(32, 32, 32)"; // TODO maybe not the best idea...
chart.Chart.defaults.color = "rgb(191, 191, 191)";

/**
 * Class for showing information for debugging performance/etc.
 */
export class PerformanceChart {
    constructor(latencyCanvas: HTMLCanvasElement, rateCanvas: HTMLCanvasElement) {
        /* Function to append a dataset to a timeline chart. */
        const appendTimelineDataset = (c: chart.Chart, label: string, colour: string): void => {
            c.data.datasets.splice(c.data.datasets.length - this.getTimelineConfig().data.datasets.length, 0, {
                label: label,
                pointStyle: "line",
                backgroundColor: colour,
                borderColor: colour,
                data: []
            });
        };

        /* Latency chart. */
        const latencyConfig = this.getTimelineConfig();
        latencyConfig.options!.scales = {
            ...latencyConfig.options!.scales,
            y: {
                title: {
                    text: "milliseconds",
                    display: true
                },
                position: "left" as const,
                type: "linear" as const,
                beginAtZero: true
            }
        };
        this.latencyChart = new chart.Chart(latencyCanvas, latencyConfig);
        appendTimelineDataset(this.latencyChart, "Buffer", "rgb(255, 233, 63)");
        appendTimelineDataset(this.latencyChart, "Network Latency", "rgb(0, 255, 0)");
        this.latencyChart.update();

        /* Download rate chart. */
        const dlConfig = this.getTimelineConfig();
        dlConfig.options!.scales = {
            ...dlConfig.options!.scales,
            y: {
                title: {
                    text: "kilobits per second",
                    display: true
                },
                type: "linear" as const,
                position: "left" as const,
                beginAtZero: true
            }
        };
        this.dlChart = new chart.Chart(rateCanvas, dlConfig);
        appendTimelineDataset(this.dlChart, "Download Rate", "rgb(127, 127, 255)");
        appendTimelineDataset(this.dlChart, "Average Download Rate", "rgb(0,127,106)");
        this.dlChart.update();

        /* Offset all timestamps by this amount. */
        this.start = Date.now();
    }

    destroy(): void {
        this.latencyChart.destroy();
        this.dlChart.destroy();
    }

    setBufferControl(_bctrl: BufferControl): void {
        // noop
    }

    onBufferControlTick(tickInfo: BufferControlTickInfo): void {
        /* Calculate the timestamp. */
        const t = (tickInfo.timestamp - this.start) / 1000;

        this.getData(this.latencyChart, "Buffer").push({x: t, y: tickInfo.primaryBufferLength});

        /* We have a new timeline data point. */
        this.updateTimelines(tickInfo.timestamp);
    }

    onTimestamp(timestampInfo: TimestampInfo): void {
        const t = (timestampInfo.sentTimestamp - this.start) / 1000;
        const d = timestampInfo.endReceivedTimestamp - timestampInfo.sentTimestamp;
        this.getData(this.latencyChart, "Network Latency").push({x: t, y: d});
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


    /* Common configuration between timelines. */
    private getTimelineConfig(): chart.ChartConfiguration {
        return {
            type: "line" as const,
            data: {
                datasets: []
            },
            options: {
                animation: false as const,
                aspectRatio: 4,
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
                    }
                },
                plugins: {
                    legend: {
                        labels: {
                            usePointStyle: true
                        }
                    }
                }
            }
        };
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
        const max = Math.max(this.timelineLatest / 1000, 120);
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
