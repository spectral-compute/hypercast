import * as chart from "chart.js";
import {BufferControl, BufferControlTickInfo, TimestampInfo} from "live-video-streamer-client";

chart.Chart.register(...chart.registerables);

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
        appendTimelineDataset(this.latencyChart, "Network Latency", "rgb(0, 0, 255)");
        this.latencyChart.update();

        /* Offset all timestamps by this amount. */
        this.start = Date.now();
    }

    setBufferControl(bctrl: BufferControl): void {
        this.bctrl = bctrl;
    }

    onBufferControlTick(tickInfo: BufferControlTickInfo): void {
        /* A hack to avoid showing a huge apparent buffer length at the start of streaming. */
        if (tickInfo.timestamp < this.start + 4000) {
            return;
        }

        /* Calculate the timestamp. */
        const t = (tickInfo.timestamp - this.start) / 1000;

        /* Add the various data points. */
        if (tickInfo.catchUp) {
            for (const c of [this.latencyChart]) {
                this.getData(c, "Catch Up Event").push({x: t, y: 0});
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
            for (const c of [this.latencyChart]) {
                this.getData(c, "Segment Start").push({x: t, y: 0});
            }
        }
        this.updateTimelines(t);
    }

    /**
     * Get the data array for the given chart/label pair.
     *
     * @param chart The chart to get the data array from.
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
        for (const c of [this.latencyChart]) {
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
        for (const c of [this.latencyChart]) {
            c.update();
        }
    }

    private bctrl!: BufferControl;

    private readonly start: number; // Timestamp of what we're calling time 0.
    private timelineLatest: number = 0; // The time of the latest data point.

    private readonly latencyChart: chart.Chart; // Chart to show the latency timeline.
}
