function isObject(v) {
    return typeof v === 'object' && v !== null && !Array.isArray(v);
}
/**
 * Computes a flat diff between two JSON-compatible values.
 *
 * For objects, recurses into nested properties.
 * For arrays and primitives, compares the JSON representation directly.
 */
export function computeDiff(before, after, prefix = '') {
    if (isObject(before) && isObject(after)) {
        const keys = new Set([...Object.keys(before), ...Object.keys(after)]);
        const entries = [];
        for (const key of keys) {
            const path = prefix ? `${prefix}.${key}` : key;
            entries.push(...computeDiff(before[key], after[key], path));
        }
        return entries;
    }
    const beforeStr = JSON.stringify(before) ?? 'null';
    const afterStr = JSON.stringify(after) ?? 'null';
    return [
        {
            path: prefix || '.',
            before: beforeStr,
            after: afterStr,
            changed: beforeStr !== afterStr,
        },
    ];
}
/** Filters a diff result to only changed entries. */
export function changedEntries(diff) {
    return diff.filter((d) => d.changed);
}
/** Renders a diff as a Markdown table string (used for logging and debug). */
export function renderDiffTable(diff) {
    const rows = diff.map((d) => `| ${d.path} | ${d.before} | ${d.after} | ${d.changed ? '✓' : ''} |`);
    return [
        '| Path | Before | After | Changed |',
        '|------|--------|-------|---------|',
        ...rows,
    ].join('\n');
}
/** Parses a preview_result_json string and computes the diff against an empty baseline. */
export function diffFromPreviewJson(previewJson) {
    try {
        const parsed = JSON.parse(previewJson);
        if (isObject(parsed) && 'before' in parsed && 'after' in parsed) {
            return computeDiff(parsed['before'], parsed['after']);
        }
        return computeDiff(null, parsed);
    }
    catch {
        return [];
    }
}
//# sourceMappingURL=diff.js.map