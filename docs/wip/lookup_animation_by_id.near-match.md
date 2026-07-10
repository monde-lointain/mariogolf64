# lookup_animation_by_id (0x80055FFC) — SOLVED / BANKED S213

Banked (commit "Bank lookup_animation_by_id ... (S213)"). Root-caused via a
compiler-source dive (loop.c rotation + reorg.c fill_eager_delay_slots).

## The lever (reusable)
Top-tested search loops: frame the `while(...)` condition on the LIST-TERMINATOR
/continue test, NOT the found test; keep the found-check as an interior
`if(){...goto done;}`. gcc-2.7.2 then emits all three target properties at once:
- entry-guard hoists the counter-init into its delay slot (no loop.c rotation),
- the bottom re-test becomes a conditional `bnez` back-edge,
- reorg annuls the advance into a `bnel` inline compare.
do-while (entry beqz skips the return-tail -> duplicate `move v0,aN`) and
found-as-condition (rotates counter inside) both FAIL. The prior "rotation vs
annul mutually exclusive" wall was a wrong loop framing, not a real wall.
Frame fix (still required): init `result=0` AFTER the get_character_state call.
