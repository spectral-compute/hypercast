import {expect, test} from "@jest/globals";
import {assertNonNull} from "../src/Assertion";

function assertNonNullAsserts(value: any): boolean {
    try {
        assertNonNull(value);
        return false;
    } catch (e: any) {
        return true;
    }
}

test("undefined", (): void => {
    expect(assertNonNullAsserts(undefined)).toBeTruthy();
});

test("null", (): void => {
    expect(assertNonNullAsserts(null)).toBeTruthy();
});

test("0", (): void => {
    expect(assertNonNullAsserts(0)).toBeFalsy();
});

test("42", (): void => {
    expect(assertNonNullAsserts(42)).toBeFalsy();
});
