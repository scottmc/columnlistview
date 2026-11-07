# ColumnListViewDemo

A test bench for the latch (the expand/collapse widget) in Haiku's private
`BColumnListView`. The tree is on the left, the experiments are on the right,
and the log at the bottom right records what the list actually does.

## Build

On Haiku, in this folder:

    make
    ./ColumnListViewDemo

`BColumnListView` is a private class, so the Makefile adds
`/boot/system/develop/headers/private/interface` to the include path.

## What it probes

| Experiment | Question it answers |
|---|---|
| Click a latch / Left-Right keys vs. the Expand/Collapse buttons | Does the list call the public virtual `ExpandOrCollapse()` itself, or only when you call it? |
| Latch width spinner (`SetLatchWidth`) | Does it change the latch, the indent, or both? Can it be 0? |
| Move Name to end / Size to start | Which column carries the latch and indent: the first logical column, or the first one on screen? |
| Hide Name column | What happens to the latch when its column is gone? |
| Custom drawing styles (`DrawLatch` override) | How much of the look can you change? Does the click area follow the drawing? |
| Circle on lazy rows only | Can `DrawLatch` treat individual rows differently (it receives the `BRow*`)? |
| Lazy folder + "override HasLatch()" | Is the virtual `BRow::HasLatch()` consulted, so a childless row can show a latch for on-demand loading? |
| Sorting enabled | Do children stay under their parents when sorting by a header? |
| Tall rows + rebuild | Does the latch stay vertically centred on rows taller than the default? |
| "DrawLatch probe" log lines | Exact rectangle the latch is drawn in, after any change above. |

## Status

Written without access to a Haiku machine, so it has not been compiled or run.
The API calls were checked against the r1beta4 `ColumnListView.h`. Expect
possible small fixes for compile errors on first build. The behaviour of
the latch itself is deliberately not asserted anywhere: the log reports what
happens.
