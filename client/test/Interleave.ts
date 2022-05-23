import {expect, test} from "@jest/globals";
import {Deinterleaver} from "../src/Deinterleave";

function getDeinterleaveAndBuffers(): [Deinterleaver, Uint8Array[][]] {
    const buffers: Uint8Array[][] = [[], []];
    const deinterleaver = new Deinterleaver((data: ArrayBuffer, index: number): void => {
        expect(index === 0 || index === 1).toBeTruthy();
        buffers[index]!.push(new Uint8Array(data));
    }, "");
    return [deinterleaver, buffers];
}

test("Simple", (): void => {
    const [deinterleaver, buffers] = getDeinterleaveAndBuffers();
    deinterleaver.acceptData(Uint8Array.from([
        0x00, 0x04, 0x00, 0x01, 0x02, 0x03, // Part A-0.
        0x01, 0x04, 0x10, 0x11, 0x12, 0x13, // Part B-0.
        0x00, 0x04, 0x04, 0x05, 0x06, 0x07, // Part A-1.
        0x01, 0x04, 0x14, 0x15, 0x16, 0x17, // Part B-1.
        0x00, 0x00, 0x01, 0x00 // Parts A and B EOF.
    ]));

    expect(buffers).toStrictEqual([
        [
            Uint8Array.from([0x00, 0x01, 0x02, 0x03]), // Part A-0.
            Uint8Array.from([0x04, 0x05, 0x06, 0x07]), // Part A-1.
            Uint8Array.from([]) // Part A EOF.
        ],
        [
            Uint8Array.from([0x10, 0x11, 0x12, 0x13]), // Part B-0.
            Uint8Array.from([0x14, 0x15, 0x16, 0x17]), // Part B-1.
            Uint8Array.from([]) // Part B EOF.
        ]
    ]);
});

test("Byte at a time", (): void => {
    const [deinterleaver, buffers] = getDeinterleaveAndBuffers();
    for (const n of [
        0x00, 0x04, 0x00, 0x01, 0x02, 0x03, // Part A-0.
        0x01, 0x04, 0x10, 0x11, 0x12, 0x13, // Part B-0.
        0x00, 0x04, 0x04, 0x05, 0x06, 0x07, // Part A-1.
        0x01, 0x04, 0x14, 0x15, 0x16, 0x17, // Part B-1.
        0x00, 0x00, 0x01, 0x00 // End of file for both A and B.
    ]) {
        deinterleaver.acceptData(Uint8Array.from([n]));
    }

    expect(buffers).toStrictEqual([
        [
            Uint8Array.from([0x00]), // Part A-0.
            Uint8Array.from([0x01]),
            Uint8Array.from([0x02]),
            Uint8Array.from([0x03]),
            Uint8Array.from([0x04]), // Part A-1.
            Uint8Array.from([0x05]),
            Uint8Array.from([0x06]),
            Uint8Array.from([0x07]),
            Uint8Array.from([]) // Part A EOF.
        ],
        [
            Uint8Array.from([0x10]), // Part B-0.
            Uint8Array.from([0x11]),
            Uint8Array.from([0x12]),
            Uint8Array.from([0x13]),
            Uint8Array.from([0x14]), // Part B-1.
            Uint8Array.from([0x15]),
            Uint8Array.from([0x16]),
            Uint8Array.from([0x17]),
            Uint8Array.from([]) // Part B EOF.
        ]
    ]);
});

test("Exact chunks", (): void => {
    const [deinterleaver, buffers] = getDeinterleaveAndBuffers();
    deinterleaver.acceptData(Uint8Array.from([0x00, 0x04, 0x00, 0x01, 0x02, 0x03])); // Part A-0.
    deinterleaver.acceptData(Uint8Array.from([0x01, 0x04, 0x10, 0x11, 0x12, 0x13])); // Part B-0.
    deinterleaver.acceptData(Uint8Array.from([0x00, 0x04, 0x04, 0x05, 0x06, 0x07])); // Part A-1.
    deinterleaver.acceptData(Uint8Array.from([0x01, 0x04, 0x14, 0x15, 0x16, 0x17])); // Part B-1.
    deinterleaver.acceptData(Uint8Array.from([0x00, 0x00])); // Part A EOF.
    deinterleaver.acceptData(Uint8Array.from([0x01, 0x00])); // Part B EOF.

    expect(buffers).toStrictEqual([
        [
            Uint8Array.from([0x00, 0x01, 0x02, 0x03]), // Part A-0.
            Uint8Array.from([0x04, 0x05, 0x06, 0x07]), // Part A-1.
            Uint8Array.from([]) // Part A EOF.
        ],
        [
            Uint8Array.from([0x10, 0x11, 0x12, 0x13]), // Part B-0.
            Uint8Array.from([0x14, 0x15, 0x16, 0x17]), // Part B-1.
            Uint8Array.from([]) // Part B EOF.
        ]
    ]);
});

test("Straddle chunks", (): void => {
    const [deinterleaver, buffers] = getDeinterleaveAndBuffers();
    deinterleaver.acceptData(Uint8Array.from([0x00, 0x04, 0x00, 0x01, 0x02, 0x03, 0x01, 0x04]));
    deinterleaver.acceptData(Uint8Array.from([0x10, 0x11, 0x12, 0x13, 0x00]));
    deinterleaver.acceptData(Uint8Array.from([0x04, 0x04, 0x05, 0x06, 0x07, 0x01, 0x04, 0x14]));
    deinterleaver.acceptData(Uint8Array.from([0x15, 0x16, 0x17, 0x00, 0x00, 0x01, 0x00]));

    expect(buffers).toStrictEqual([
        [
            Uint8Array.from([0x00, 0x01, 0x02, 0x03]), // Part A-0.
            Uint8Array.from([0x04, 0x05, 0x06, 0x07]), // Part A-1.
            Uint8Array.from([]) // Part A EOF.
        ],
        [
            Uint8Array.from([0x10, 0x11, 0x12, 0x13]), // Part B-0.
            Uint8Array.from([0x14]), // Part B-1.
            Uint8Array.from([0x15, 0x16, 0x17]),
            Uint8Array.from([]) // Part B EOF.
        ]
    ]);
});

test("Early EOF", (): void => {
    const [deinterleaver, buffers] = getDeinterleaveAndBuffers();
    deinterleaver.acceptData(Uint8Array.from([
        0x00, 0x04, 0x00, 0x01, 0x02, 0x03, // Part A-0.
        0x01, 0x04, 0x10, 0x11, 0x12, 0x13, // Part B-0.
        0x00, 0x04, 0x04, 0x05, 0x06, 0x07, // Part A-1.
        0x00, 0x00, // Part A EOF.
        0x01, 0x04, 0x14, 0x15, 0x16, 0x17, // Part B-1.
        0x01, 0x00 // Part B EOF.
    ]));

    expect(buffers).toStrictEqual([
        [
            Uint8Array.from([0x00, 0x01, 0x02, 0x03]), // Part A-0.
            Uint8Array.from([0x04, 0x05, 0x06, 0x07]), // Part A-1.
            Uint8Array.from([]) // Part A EOF.
        ],
        [
            Uint8Array.from([0x10, 0x11, 0x12, 0x13]), // Part B-0.
            Uint8Array.from([0x14, 0x15, 0x16, 0x17]), // Part B-1.
            Uint8Array.from([]) // Part B EOF.
        ]
    ]);
});

test("16-bit chunk length", (): void => {
    const a1Data = new Uint8Array(10000);
    for (let i: number = 0; i < a1Data.length; i++) {
        a1Data[i] = (i * 7) % 256;
    }

    const [deinterleaver, buffers] = getDeinterleaveAndBuffers();
    deinterleaver.acceptData(Uint8Array.from([
        0x00, 0x04, 0x00, 0x01, 0x02, 0x03, // Part A-0.
        0x40, 0x10, 0x27 // Part A-1.
    ]));
    deinterleaver.acceptData(a1Data);
    deinterleaver.acceptData(Uint8Array.from([
        0x00, 0x04, 0x04, 0x05, 0x06, 0x07, // Part A-2.
        0x01, 0x04, 0x10, 0x11, 0x12, 0x13, // Part B-0.
        0x00, 0x00, 0x01, 0x00 // Parts A and B EOF.
    ]));

    expect(buffers).toStrictEqual([
        [
            Uint8Array.from([0x00, 0x01, 0x02, 0x03]), // Part A-0.
            a1Data, // Part A-1.
            Uint8Array.from([0x04, 0x05, 0x06, 0x07]), // Part A-2.
            Uint8Array.from([]) // Part A EOF.
        ],
        [
            Uint8Array.from([0x10, 0x11, 0x12, 0x13]), // Part B-0.
            Uint8Array.from([]) // Part B EOF.
        ]
    ]);
});

test("32-bit chunk length", (): void => {
    const b1Data = new Uint8Array(100000);
    for (let i: number = 0; i < b1Data.length; i++) {
        b1Data[i] = (i * 17) % 256;
    }

    const [deinterleaver, buffers] = getDeinterleaveAndBuffers();
    deinterleaver.acceptData(Uint8Array.from([
        0x00, 0x04, 0x00, 0x01, 0x02, 0x03, // Part A-0.
        0x01, 0x04, 0x10, 0x11, 0x12, 0x13, // Part B-0.
        0x81, 0xA0, 0x86, 0x01, 0x00 // Part B-1.
    ]));
    deinterleaver.acceptData(b1Data);
    deinterleaver.acceptData(Uint8Array.from([
        0x01, 0x04, 0x14, 0x15, 0x16, 0x17, // Part B-2.
        0x00, 0x00, 0x01, 0x00 // Parts A and B EOF.
    ]));

    expect(buffers).toStrictEqual([
        [
            Uint8Array.from([0x00, 0x01, 0x02, 0x03]), // Part A-0.
            Uint8Array.from([]) // Part A EOF.
        ],
        [
            Uint8Array.from([0x10, 0x11, 0x12, 0x13]), // Part B-0.
            b1Data, // Part B-1.
            Uint8Array.from([0x14, 0x15, 0x16, 0x17]), // Part B-2.
            Uint8Array.from([]) // Part B EOF.
        ]
    ]);
});
