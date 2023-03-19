// Because these aren't included in the library...
export function setUnion<T>(a: Set<T>, b: Set<T>) {
    return new Set([...Array.from(a), ...Array.from(b)]);
}
export function setIntersection<T>(a: Set<T>, b: Set<T>) {
    return new Set(Array.from(a).filter(x => b.has(x)));
}
export function setDifference<T>(a: Set<T>, b: Set<T>) {
    return new Set(Array.from(a).filter(x => !b.has(x)));
}
