import {expect, test} from "@jest/globals";
import {ServerFileStore} from "../src/ServerFileStore";

function getFileAsBuffers(file: ReturnType<ServerFileStore["get"]>): Buffer[] {
    const result: Buffer[] = [];
    file.writeWith((b: Buffer): void => {
        result.push(b);
    });
    return result;
}

test("Simple", (): void => {
    /* Create a file store with an interleave. */
    const sfs = new ServerFileStore();
    sfs.addInterleavingPattern("/interleave", [/^\/a$/, /^\/b$/]);

    /* Create the underlying files. */
    expect(sfs.has("/interleave")).toBeFalsy();
    const a = sfs.add("/a");
    expect(sfs.has("/interleave")).toBeTruthy();
    const b = sfs.add("/b");

    /* Add data to both files. */
    a.add(Buffer.from([0x00, 0x01, 0x02, 0x03])); // Part A-0.
    b.add(Buffer.from([0x10, 0x11, 0x12, 0x13])); // Part B-0.
    a.add(Buffer.from([0x04, 0x05, 0x06, 0x07])); // Part A-1.
    b.add(Buffer.from([0x14, 0x15, 0x16, 0x17])); // Part B-1.

    /* Finish the underlying files.. */
    const interleave = sfs.get("/interleave");
    expect(interleave.isFinished()).toBeFalsy();
    a.finish();
    expect(interleave.isFinished()).toBeFalsy();
    b.finish();
    expect(interleave.isFinished()).toBeTruthy();

    /* Check the result. */
    expect(getFileAsBuffers(interleave)).toStrictEqual([
        Buffer.from([0x00, 0x04, 0x00, 0x01, 0x02, 0x03]), // Part A-0.
        Buffer.from([0x01, 0x04, 0x10, 0x11, 0x12, 0x13]), // Part B-0.
        Buffer.from([0x00, 0x04, 0x04, 0x05, 0x06, 0x07]), // Part A-1.
        Buffer.from([0x01, 0x04, 0x14, 0x15, 0x16, 0x17]), // Part B-1.
        Buffer.from([0x00, 0x00]), Buffer.from([0x01, 0x00]) // End of file for both A and B.
    ]);
});

test("Filename pattern", (): void => {
    /* Create a file store with an interleave. */
    const sfs = new ServerFileStore();
    sfs.addInterleavingPattern("/interleave{1}", [/^\/a([0-9]+)$/, /^\/b([0-9]+)$/]);

    /* Create the underlying files. */
    expect(sfs.has("/interleave0")).toBeFalsy();
    const a0 = sfs.add("/a0");
    expect(sfs.has("/interleave0")).toBeTruthy();
    const b0 = sfs.add("/b0");

    expect(sfs.has("/interleave1")).toBeFalsy();
    const a1 = sfs.add("/a1");
    expect(sfs.has("/interleave1")).toBeTruthy();
    const b1 = sfs.add("/b1");

    /* Add data to both files. */
    a0.add(Buffer.from([0x00, 0x01, 0x02, 0x03])); // Part A0-0.
    b0.add(Buffer.from([0x10, 0x11, 0x12, 0x13])); // Part B0-0.
    a0.add(Buffer.from([0x04, 0x05, 0x06, 0x07])); // Part A0-1.
    b0.add(Buffer.from([0x14, 0x15, 0x16, 0x17])); // Part B0-1.

    a1.add(Buffer.from([0x20, 0x21, 0x22, 0x23])); // Part A1-0.
    b1.add(Buffer.from([0x30, 0x31, 0x32, 0x33])); // Part B1-0.
    a1.add(Buffer.from([0x24, 0x25, 0x26, 0x27])); // Part A1-1.
    b1.add(Buffer.from([0x34, 0x35, 0x36, 0x37])); // Part B1-1.

    /* Finish the underlying files. */
    const interleave0 = sfs.get("/interleave0");
    expect(interleave0.isFinished()).toBeFalsy();
    a0.finish();
    expect(interleave0.isFinished()).toBeFalsy();
    b0.finish();
    expect(interleave0.isFinished()).toBeTruthy();

    const interleave1 = sfs.get("/interleave1");
    expect(interleave1.isFinished()).toBeFalsy();
    a1.finish();
    expect(interleave1.isFinished()).toBeFalsy();
    b1.finish();
    expect(interleave1.isFinished()).toBeTruthy();

    /* Check the result. */
    expect(getFileAsBuffers(interleave0)).toStrictEqual([
        Buffer.from([0x00, 0x04, 0x00, 0x01, 0x02, 0x03]), // Part A0-0.
        Buffer.from([0x01, 0x04, 0x10, 0x11, 0x12, 0x13]), // Part B0-0.
        Buffer.from([0x00, 0x04, 0x04, 0x05, 0x06, 0x07]), // Part A0-1.
        Buffer.from([0x01, 0x04, 0x14, 0x15, 0x16, 0x17]), // Part B0-1.
        Buffer.from([0x00, 0x00]), Buffer.from([0x01, 0x00]) // End of file for both A0 and B0.
    ]);
    expect(getFileAsBuffers(interleave1)).toStrictEqual([
        Buffer.from([0x00, 0x04, 0x20, 0x21, 0x22, 0x23]), // Part A1-0.
        Buffer.from([0x01, 0x04, 0x30, 0x31, 0x32, 0x33]), // Part B1-0.
        Buffer.from([0x00, 0x04, 0x24, 0x25, 0x26, 0x27]), // Part A1-1.
        Buffer.from([0x01, 0x04, 0x34, 0x35, 0x36, 0x37]), // Part B1-1.
        Buffer.from([0x00, 0x00]), Buffer.from([0x01, 0x00]) // End of file for both A0 and B0.
    ]);
});

test("Successive chunk", (): void => {
    /* Create a file store with an interleave. */
    const sfs = new ServerFileStore();
    sfs.addInterleavingPattern("/interleave", [/^\/a$/, /^\/b$/]);

    /* Create the underlying files. */
    expect(sfs.has("/interleave")).toBeFalsy();
    const a = sfs.add("/a");
    expect(sfs.has("/interleave")).toBeTruthy();
    const b = sfs.add("/b");

    /* Add data to both files. */
    a.add(Buffer.from([0x00, 0x01, 0x02, 0x03])); // Part A-0.
    a.add(Buffer.from([0x04, 0x05, 0x06, 0x07])); // Part A-1.
    b.add(Buffer.from([0x10, 0x11, 0x12, 0x13])); // Part B-0.
    b.add(Buffer.from([0x14, 0x15, 0x16, 0x17])); // Part B-1.

    /* Finish the underlying files.. */
    const interleave = sfs.get("/interleave");
    expect(interleave.isFinished()).toBeFalsy();
    a.finish();
    expect(interleave.isFinished()).toBeFalsy();
    b.finish();
    expect(interleave.isFinished()).toBeTruthy();

    /* Check the result. */
    expect(getFileAsBuffers(interleave)).toStrictEqual([
        Buffer.from([0x00, 0x04, 0x00, 0x01, 0x02, 0x03]), // Part A-0.
        Buffer.from([0x00, 0x04, 0x04, 0x05, 0x06, 0x07]), // Part A-1.
        Buffer.from([0x01, 0x04, 0x10, 0x11, 0x12, 0x13]), // Part B-0.
        Buffer.from([0x01, 0x04, 0x14, 0x15, 0x16, 0x17]), // Part B-1.
        Buffer.from([0x00, 0x00]), Buffer.from([0x01, 0x00]) // End of file for both A and B.
    ]);
});

test("Early EOF", (): void => {
    /* Create a file store with an interleave. */
    const sfs = new ServerFileStore();
    sfs.addInterleavingPattern("/interleave", [/^\/a$/, /^\/b$/]);

    /* Create the underlying files. */
    expect(sfs.has("/interleave")).toBeFalsy();
    const a = sfs.add("/a");
    expect(sfs.has("/interleave")).toBeTruthy();
    const b = sfs.add("/b");
    const interleave = sfs.get("/interleave");

    /* Add data to file A. */
    a.add(Buffer.from([0x00, 0x01, 0x02, 0x03])); // Part A-0.
    a.add(Buffer.from([0x04, 0x05, 0x06, 0x07])); // Part A-1.

    /* Finish file A. */
    expect(interleave.isFinished()).toBeFalsy();
    a.finish();
    expect(interleave.isFinished()).toBeFalsy();

    /* Add data to file B. */
    b.add(Buffer.from([0x10, 0x11, 0x12, 0x13])); // Part B-0.
    b.add(Buffer.from([0x14, 0x15, 0x16, 0x17])); // Part B-1.

    /* Finish file B. */
    expect(interleave.isFinished()).toBeFalsy();
    b.finish();
    expect(interleave.isFinished()).toBeTruthy();

    /* Check the result. */
    expect(getFileAsBuffers(interleave)).toStrictEqual([
        Buffer.from([0x00, 0x04, 0x00, 0x01, 0x02, 0x03]), // Part A-0.
        Buffer.from([0x00, 0x04, 0x04, 0x05, 0x06, 0x07]), // Part A-1.
        Buffer.from([0x00, 0x00]), // End of file A.
        Buffer.from([0x01, 0x04, 0x10, 0x11, 0x12, 0x13]), // Part B-0.
        Buffer.from([0x01, 0x04, 0x14, 0x15, 0x16, 0x17]), // Part B-1.
        Buffer.from([0x01, 0x00]) // End of file B.
    ]);
});

test("16-bit chunk length", (): void => {
    /* Create a file store with an interleave. */
    const sfs = new ServerFileStore();
    sfs.addInterleavingPattern("/interleave", [/^\/a$/, /^\/b$/]);

    /* Create the underlying files. */
    expect(sfs.has("/interleave")).toBeFalsy();
    const a = sfs.add("/a");
    expect(sfs.has("/interleave")).toBeTruthy();
    const b = sfs.add("/b");

    /* Add data to both files. */
    const a1Data = Buffer.alloc(10000);
    for (let i: number = 0; i < a1Data.length; i++) {
        a1Data[i] = (i * 7) % 256;
    }

    a.add(Buffer.from([0x00, 0x01, 0x02, 0x03])); // Part A-0.
    a.add(a1Data); // Part A-1.
    a.add(Buffer.from([0x04, 0x05, 0x06, 0x07])); // Part A-2.
    b.add(Buffer.from([0x10, 0x11, 0x12, 0x13])); // Part B-0.

    /* Finish the underlying files.. */
    const interleave = sfs.get("/interleave");
    expect(interleave.isFinished()).toBeFalsy();
    a.finish();
    expect(interleave.isFinished()).toBeFalsy();
    b.finish();
    expect(interleave.isFinished()).toBeTruthy();

    /* Check the result. */
    expect(getFileAsBuffers(interleave)).toStrictEqual([
        Buffer.from([0x00, 0x04, 0x00, 0x01, 0x02, 0x03]), // Part A-0.
        Buffer.concat([Buffer.from([0x40, 0x10, 0x27]), a1Data]), // Part A-1.
        Buffer.from([0x00, 0x04, 0x04, 0x05, 0x06, 0x07]), // Part A-2.
        Buffer.from([0x01, 0x04, 0x10, 0x11, 0x12, 0x13]), // Part B-2.
        Buffer.from([0x00, 0x00]), Buffer.from([0x01, 0x00]) // End of file for both A and B.
    ]);
});

test("32-bit chunk length", (): void => {
    /* Create a file store with an interleave. */
    const sfs = new ServerFileStore();
    sfs.addInterleavingPattern("/interleave", [/^\/a$/, /^\/b$/]);

    /* Create the underlying files. */
    expect(sfs.has("/interleave")).toBeFalsy();
    const a = sfs.add("/a");
    expect(sfs.has("/interleave")).toBeTruthy();
    const b = sfs.add("/b");

    /* Add data to both files. */
    const b1Data = Buffer.alloc(100000);
    for (let i: number = 0; i < b1Data.length; i++) {
        b1Data[i] = (i * 17) % 256;
    }

    a.add(Buffer.from([0x00, 0x01, 0x02, 0x03])); // Part A-0.
    b.add(Buffer.from([0x10, 0x11, 0x12, 0x13])); // Part B-0.
    b.add(b1Data); // Part B-1.
    b.add(Buffer.from([0x14, 0x15, 0x16, 0x17])); // Part B-2.

    /* Finish the underlying files.. */
    const interleave = sfs.get("/interleave");
    expect(interleave.isFinished()).toBeFalsy();
    a.finish();
    expect(interleave.isFinished()).toBeFalsy();
    b.finish();
    expect(interleave.isFinished()).toBeTruthy();

    /* Check the result. */
    expect(getFileAsBuffers(interleave)).toStrictEqual([
        Buffer.from([0x00, 0x04, 0x00, 0x01, 0x02, 0x03]), // Part A-0.
        Buffer.from([0x01, 0x04, 0x10, 0x11, 0x12, 0x13]), // Part B-0.
        Buffer.concat([Buffer.from([0x81, 0xA0, 0x86, 0x01, 0x00]), b1Data]), // Part B-1.
        Buffer.from([0x01, 0x04, 0x14, 0x15, 0x16, 0x17]), // Part B-2.
        Buffer.from([0x00, 0x00]), Buffer.from([0x01, 0x00]) // End of file for both A and B.
    ]);
});
