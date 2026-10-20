/**
 * mg64-unfinished-run: keeps a nigel decompile session going when it stops before its run has an
 * outcome (`difficulty_ledger.py pending`). Haiku sometimes ends its turn with a plan instead of
 * a tool call, most often right after a compaction; nigel then discards a run with no attempt.
 * Applies only under a `decompile-*` nigel task with more than 5 minutes to its deadline, so
 * interactive sessions are untouched. omp caps continuations at 8 per prompt.
 */
import { execFileSync } from "node:child_process";
import { realpathSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import type { ExtensionAPI } from "@oh-my-pi/pi-coding-agent";

const REPO_ROOT = path.resolve(realpathSync(path.dirname(fileURLToPath(import.meta.url))), "../../..");
const MIN_SECONDS_LEFT = 300;

const CONTINUE_MESSAGE =
	"The run has no outcome yet: no match integrated, no `STOP:` from build.sh, no NEAR_MISS.md and no ledger record. Continue from the next step of the environment's AGENTS.md now, with a tool call.";

function pendingEnv(env: NodeJS.ProcessEnv, nowSeconds: number): string | undefined {
	const { NIGEL_TASK_NAME: task, NIGEL_CANDIDATE: fn, NIGEL_RUN_ID: runId } = env;
	if (!task?.startsWith("decompile-") || !fn || !runId) return undefined;
	const deadline = Number(env.NIGEL_TIMEOUT_DEADLINE_UNIX);
	if (!(deadline - nowSeconds > MIN_SECONDS_LEFT)) return undefined;
	return path.join(REPO_ROOT, "nonmatchings", `${fn}-${runId}`);
}

export default function mg64UnfinishedRun(pi: ExtensionAPI): void {
	pi.on("session_stop", async () => {
		const env = pendingEnv(process.env, Date.now() / 1000);
		if (env === undefined) return undefined;
		let state: string;
		try {
			state = execFileSync("python3", ["tools/difficulty_ledger.py", "pending", env], {
				cwd: REPO_ROOT,
				encoding: "utf8",
				stdio: ["ignore", "pipe", "ignore"],
			}).trim();
		} catch {
			return undefined;
		}
		return state === "pending" ? { continue: true, additionalContext: CONTINUE_MESSAGE } : undefined;
	});
}
