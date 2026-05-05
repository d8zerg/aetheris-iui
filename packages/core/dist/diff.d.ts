/** A single diff entry representing a field change between two JSON values. */
export interface DiffEntry {
    readonly path: string;
    readonly before: string;
    readonly after: string;
    readonly changed: boolean;
}
/**
 * Computes a flat diff between two JSON-compatible values.
 *
 * For objects, recurses into nested properties.
 * For arrays and primitives, compares the JSON representation directly.
 */
export declare function computeDiff(before: unknown, after: unknown, prefix?: string): DiffEntry[];
/** Filters a diff result to only changed entries. */
export declare function changedEntries(diff: readonly DiffEntry[]): readonly DiffEntry[];
/** Renders a diff as a Markdown table string (used for logging and debug). */
export declare function renderDiffTable(diff: readonly DiffEntry[]): string;
/** Parses a preview_result_json string and computes the diff against an empty baseline. */
export declare function diffFromPreviewJson(previewJson: string): DiffEntry[];
//# sourceMappingURL=diff.d.ts.map