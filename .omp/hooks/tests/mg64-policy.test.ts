import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { realpathSync } from "node:fs";
import path from "node:path";
import mg64Policy from "../pre/mg64-policy.ts";

type ToolCallResult = { block?: boolean; reason?: string } | undefined;
type ToolCallHandler = (event: { toolName: string; input: unknown }, ctx: { cwd: string }) => Promise<ToolCallResult>;

let handler: ToolCallHandler | undefined;
mg64Policy({
	on(name: string, fn: ToolCallHandler) {
		if (name === "tool_call") handler = fn;
	},
} as never);

const REPO = realpathSync(path.resolve(import.meta.dir, "../../.."));
const DECOMP_ENV = path.join(REPO, "nonmatchings", "fn");

const CTX = "ctx.c is an automatically generated file";
const TEMP = "temp.c is an automatically generated file";
const SHA1 = "mariogolf64.sha1 is a reference checksum file";
const ASM_WRITE = "must not create, edit, move, or delete files in asm/";
const ASM_BASH = "Read them for analysis";
const VENDORED = "KMC toolchain";
const MAKE = "Direct 'make' commands are not allowed";
const SPLAT = "Direct 'python3 -m splat split' commands are not allowed";
const NO_VERIFY = "matches denied pattern 'git\\s+commit\\s+--no-verify'";
const NO_TIMEOUT = "must be wrapped with a valid shell timeout";
const PARSE = "unable to parse command while checking permuter timeout";
const AST_PARSE = "unable to parse ast_edit payload";

async function decide(tool: string, input: unknown, cwd = REPO): Promise<string | undefined> {
	if (handler === undefined) throw new Error("tool_call handler was not registered");
	const result = await handler({ toolName: tool, input }, { cwd });
	return result?.block === true ? result.reason : undefined;
}

type Case = { name: string; tool: string; input: unknown; cwd?: string; blocked?: string };

const cases: Case[] = [
	// read
	{ name: "read: ctx.c with line selector is blocked", tool: "read", input: { path: "ctx.c:1-20" }, blocked: CTX },
	{ name: "read: ctx.c with raw selector is blocked", tool: "read", input: { path: "ctx.c:raw:2-4" }, blocked: CTX },
	{ name: "read: temp.c is blocked", tool: "read", input: { path: "temp.c" }, blocked: TEMP },
	{ name: "read: source file is allowed", tool: "read", input: { path: "src/main/foo.c" } },
	{ name: "read: asm file with selector is allowed", tool: "read", input: { path: "asm/foo.s:10-20" } },
	{ name: "read: sha1 file is allowed", tool: "read", input: { path: "mariogolf64.sha1" } },
	{ name: "read: skill URL is allowed", tool: "read", input: { path: "skill://mg64-decomp" } },
	{ name: "read: sqlite table selector is allowed", tool: "read", input: { path: "db.sqlite:table" } },

	// write
	{ name: "write: ctx.c is blocked", tool: "write", input: { path: "src/main/ctx.c", content: "" }, blocked: CTX },
	{ name: "write: temp.c is blocked", tool: "write", input: { path: "temp.c", content: "" }, blocked: TEMP },
	{ name: "write: sha1 is blocked", tool: "write", input: { path: "mariogolf64.sha1", content: "" }, blocked: SHA1 },
	{ name: "write: asm file is blocked", tool: "write", input: { path: "asm/foo.s", content: "" }, blocked: ASM_WRITE },
	{ name: "write: nested build/asm file is blocked", tool: "write", input: { path: "build/asm/x.s", content: "" }, blocked: ASM_WRITE },
	{
		name: "write: asm reached via relative path from decomp env is blocked",
		tool: "write",
		input: { path: "../../asm/x.s", content: "" },
		cwd: DECOMP_ENV,
		blocked: ASM_WRITE,
	},
	{
		name: "write: ctx.c relative to decomp env is blocked",
		tool: "write",
		input: { path: "ctx.c", content: "" },
		cwd: DECOMP_ENV,
		blocked: CTX,
	},
	{ name: "write: tools/kmc-cc is blocked", tool: "write", input: { path: "tools/kmc-cc", content: "" }, blocked: VENDORED },
	{ name: "write: tools/gcc_kmc file is blocked", tool: "write", input: { path: "tools/gcc_kmc/x.c", content: "" }, blocked: VENDORED },
	{
		name: "write: tools/decomp-permuter file is blocked",
		tool: "write",
		input: { path: "tools/decomp-permuter/permuter.py", content: "" },
		blocked: VENDORED,
	},
	{ name: "write: source .s file is allowed", tool: "write", input: { path: "src/libultra/os/foo.s", content: "" } },
	{ name: "write: outside repo asm path is allowed", tool: "write", input: { path: "/tmp/asm/x.s", content: "" } },
	{ name: "write: outside repo ctx.c is blocked", tool: "write", input: { path: "/tmp/ctx.c", content: "" }, blocked: CTX },

	// edit
	{
		name: "edit replace: asm path is blocked",
		tool: "edit",
		input: { path: "asm/nonmatchings/a.s", old_string: "a", new_string: "b" },
		blocked: ASM_WRITE,
	},
	{
		name: "edit patch: vendored path in paths[] is blocked",
		tool: "edit",
		input: { paths: ["tools/asm-differ/x.py"] },
		blocked: VENDORED,
	},
	{
		name: "edit hashline: asm header is blocked",
		tool: "edit",
		input: { input: "[asm/nonmatchings/a.s#ABCD]\nPUT 1.=1:\n+x" },
		blocked: ASM_WRITE,
	},
	{
		name: "edit hashline: MV into asm is blocked",
		tool: "edit",
		input: { input: "[src/main/a.c#ABCD]\nMV asm/x.s" },
		blocked: ASM_WRITE,
	},
	{
		name: "edit hashline: quoted MV into asm is blocked",
		tool: "edit",
		input: { input: '[src/main/a.c#ABCD]\nMV "asm/x.s"' },
		blocked: ASM_WRITE,
	},
	{
		name: "edit hashline: temp.c header is blocked",
		tool: "edit",
		input: { input: "[temp.c#ABCD]\nPUT 1.=1:\n+x" },
		blocked: TEMP,
	},
	{
		name: "edit hashline: MV inside a body row is not a target",
		tool: "edit",
		input: { input: "[src/main/a.c#ABCD]\nPUT 1.=1:\n+MV asm/x.s" },
	},
	{
		name: "edit hashline: source header is allowed",
		tool: "edit",
		input: { input: "[src/main/a.c#ABCD]\nPUT 1.=1:\n+int x;" },
	},
	{
		name: "edit apply_patch: update of asm file is blocked",
		tool: "edit",
		input: { input: "*** Begin Patch\n*** Update File: asm/x.s\n@@\n-a\n+b\n*** End Patch" },
		blocked: ASM_WRITE,
	},
	{
		name: "edit apply_patch: move into asm is blocked",
		tool: "edit",
		input: { input: "*** Begin Patch\n*** Update File: src/main/a.c\n*** Move to: asm/a.c\n*** End Patch" },
		blocked: ASM_WRITE,
	},
	{
		name: "edit apply_patch: delete of sha1 is blocked",
		tool: "edit",
		input: { input: "*** Begin Patch\n*** Delete File: mariogolf64.sha1\n*** End Patch" },
		blocked: SHA1,
	},
	{
		name: "edit apply_patch: add of source file is allowed",
		tool: "edit",
		input: { input: "*** Begin Patch\n*** Add File: src/main/new.c\n+int x;\n*** End Patch" },
	},

	// write xd://ast_edit and ast_edit
	{
		name: "write xd://ast_edit: asm directory entry is blocked",
		tool: "write",
		input: { path: "xd://ast_edit", content: JSON.stringify({ ops: [], paths: ["asm/"] }) },
		blocked: ASM_WRITE,
	},
	{
		name: "write xd://ast_edit: malformed payload is blocked",
		tool: "write",
		input: { path: "xd://ast_edit", content: "{" },
		blocked: AST_PARSE,
	},
	{
		name: "write xd://ast_edit: source glob is allowed",
		tool: "write",
		input: { path: "xd://ast_edit", content: JSON.stringify({ ops: [], paths: ["src/main/*.c"] }) },
	},
	{
		name: "write xd://ast_edit: glob ending in ctx.c is blocked",
		tool: "write",
		input: { path: "xd://ast_edit", content: JSON.stringify({ ops: [], paths: ["src/*/ctx.c"] }) },
		blocked: CTX,
	},
	{
		name: "ast_edit: literal asm prefix of glob is blocked",
		tool: "ast_edit",
		input: { paths: ["build/asm/*.o"] },
		blocked: ASM_WRITE,
	},
	{ name: "ast_edit: source glob is allowed", tool: "ast_edit", input: { paths: ["src/**/*.c"] } },

	// bash
	{ name: "bash: cmake is allowed", tool: "bash", input: { command: "cmake .." } },
	{ name: "bash: echo make is allowed", tool: "bash", input: { command: "echo make" } },
	{ name: "bash: bare make is blocked", tool: "bash", input: { command: "make" }, blocked: MAKE },
	{ name: "bash: make -j4 is blocked", tool: "bash", input: { command: "make -j4" }, blocked: MAKE },
	{ name: "bash: make after && is blocked", tool: "bash", input: { command: "cd x && make" }, blocked: MAKE },
	{ name: "bash: make on a later line is blocked", tool: "bash", input: { command: "echo hi\nmake" }, blocked: MAKE },
	{
		name: "bash: make -s print-cflags with SRC is allowed",
		tool: "bash",
		input: { command: "make -s print-cflags SRC=src/main/func_80025D30.c" },
	},
	{
		name: "bash: make print-cflags after cd && is allowed",
		tool: "bash",
		input: { command: "cd x && make -s --no-print-directory print-cflags SRC=src/libultra/io/foo.c | tr ' ' '\\n'" },
	},
	{
		name: "bash: print-cflags with stderr redirections is allowed",
		tool: "bash",
		input: { command: "make -s print-cflags SRC=src/main/a.c 2>&1 | head -5; make -s print-cflags SRC=src/main/b.c 2>/dev/null" },
	},
	{
		name: "bash: a redirection does not hide a real make target",
		tool: "bash",
		input: { command: "make clean 2>&1" },
		blocked: MAKE,
	},
	{
		name: "bash: print-cflags followed by a real make is blocked",
		tool: "bash",
		input: { command: "make -s print-cflags SRC=src/main/a.c && make" },
		blocked: MAKE,
	},
	{
		name: "bash: print-cflags with another target is blocked",
		tool: "bash",
		input: { command: "make -s print-cflags clean" },
		blocked: MAKE,
	},
	{
		name: "bash: print-cflags with extra flags is blocked",
		tool: "bash",
		input: { command: "make -j4 print-cflags SRC=src/main/a.c" },
		blocked: MAKE,
	},
	{
		name: "bash: print-cflags SRC with command substitution is blocked",
		tool: "bash",
		input: { command: "make -s print-cflags SRC=$(echo x)" },
		blocked: MAKE,
	},
	{
		name: "bash: git commit --no-verify is blocked",
		tool: "bash",
		input: { command: "git commit --no-verify -m x" },
		blocked: NO_VERIFY,
	},
	{ name: "bash: plain git commit is allowed", tool: "bash", input: { command: 'git commit -m "fix"' } },
	{
		name: "bash: splat split is blocked",
		tool: "bash",
		input: { command: "python3 -m splat split mg64.yaml" },
		blocked: SPLAT,
	},
	{ name: "bash: cat ctx.c is blocked", tool: "bash", input: { command: "cat ctx.c" }, blocked: CTX },
	{ name: "bash: ctx.c.m2c is blocked", tool: "bash", input: { command: "cat ctx.c.m2c" }, blocked: CTX },
	{ name: "bash: cat ctx.c in a nonmatchings dir is blocked", tool: "bash", input: { command: "cat nonmatchings/x/ctx.c" }, blocked: CTX },
	{ name: "bash: head ctx.c is blocked", tool: "bash", input: { command: "head ctx.c" }, blocked: CTX },
	{ name: "bash: grep reading ctx.c is blocked", tool: "bash", input: { command: "grep foo ctx.c" }, blocked: CTX },
	{ name: "bash: sed -i on ctx.c is blocked", tool: "bash", input: { command: "sed -i 's/a/b/' ctx.c" }, blocked: CTX },
	{ name: "bash: redirect into ctx.c is blocked", tool: "bash", input: { command: "echo x > ctx.c" }, blocked: CTX },
	{ name: "bash: cat -v ctx.c is blocked (-v is not grep's invert)", tool: "bash", input: { command: "cat -v ctx.c" }, blocked: CTX },
	{ name: "bash: rg positive glob on ctx.c is blocked", tool: "bash", input: { command: "rg foo -g ctx.c" }, blocked: CTX },
	{ name: "bash: find -name ctx.c is blocked", tool: "bash", input: { command: "find . -name ctx.c" }, blocked: CTX },
	{ name: "bash: git show of ctx.c is blocked", tool: "bash", input: { command: "git show HEAD:ctx.c" }, blocked: CTX },
	{ name: "bash: grep --exclude-from=ctx.c is blocked (it reads ctx.c)", tool: "bash", input: { command: "grep -rn foo --exclude-from=ctx.c ." }, blocked: CTX },
	{ name: "bash: grep -v with ctx.c as the file is blocked", tool: "bash", input: { command: "grep -v foo ctx.c" }, blocked: CTX },
	{ name: "bash: grep -v exclude does not hide a read of ctx.c", tool: "bash", input: { command: "grep -v foo ctx.c | grep -v ctx.c" }, blocked: CTX },
	{ name: "bash: grep -e then -v ctx.c reads ctx.c as a file", tool: "bash", input: { command: "grep -e foo -v ctx.c" }, blocked: CTX },
	{ name: "bash: grep foo -v ctx.c reads ctx.c as a file", tool: "bash", input: { command: "grep foo -v ctx.c" }, blocked: CTX },
	{ name: "bash: grep -v ctx.c with -f reads ctx.c as a file", tool: "bash", input: { command: "grep -v ctx.c -f pats.txt" }, blocked: CTX },
	{ name: "bash: grep -v with command substitution of ctx.c is blocked", tool: "bash", input: { command: 'grep -v "$(cat ctx.c)" src/a.c' }, blocked: CTX },
	{ name: "bash: grep -v with brace expansion of ctx.c is blocked", tool: "bash", input: { command: "grep -v {a,ctx.c} src/a.c" }, blocked: CTX },
	{ name: "bash: grep -v reading ctx.c through stdin redirect is blocked", tool: "bash", input: { command: "grep -v <ctx.c" }, blocked: CTX },
	{ name: "bash: grep -v with a redirect into ctx.c is blocked", tool: "bash", input: { command: "grep -v foo >ctx.c" }, blocked: CTX },
	{ name: "bash: cat ctx.c before a grep -v exclude is blocked", tool: "bash", input: { command: "cat ctx.c; grep -v ctx.c src/a.c" }, blocked: CTX },
	{ name: "bash: grep -v ctx.c on a pipe is allowed", tool: "bash", input: { command: "grep -rn foo src include | grep -v ctx.c" } },
	{ name: "bash: grep -v ctx.c on a file is allowed", tool: "bash", input: { command: "grep -n -v ctx.c src/main/a.c" } },
	{ name: "bash: grep --invert-match ctx.c on a file is allowed", tool: "bash", input: { command: "grep --invert-match ctx.c src/a.c" } },
	{ name: "bash: grep --exclude=ctx.c is allowed", tool: "bash", input: { command: "grep -rn foo --exclude=ctx.c ." } },
	{ name: "bash: grep --exclude-dir=ctx.c is allowed", tool: "bash", input: { command: "grep -rn foo --exclude-dir=ctx.c ." } },
	{ name: "bash: grep --exclude ctx.c with a space is allowed", tool: "bash", input: { command: "grep -rn foo --exclude ctx.c ." } },
	{ name: "bash: rg negated glob of ctx.c is allowed", tool: "bash", input: { command: "rg foo -g '!ctx.c'" } },
	{ name: "bash: rg --glob negated glob of ctx.c is allowed", tool: "bash", input: { command: "rg foo --glob '!ctx.c' src" } },
	{ name: "bash: find ! -name ctx.c is allowed", tool: "bash", input: { command: "find . -name '*.c' ! -name ctx.c" } },
	{ name: "bash: find -not -name ctx.c is allowed", tool: "bash", input: { command: "find . -not -name ctx.c -name '*.c'" } },
	{ name: "bash: grep pattern naming ctx.c is allowed", tool: "bash", input: { command: 'grep -n "ctx.c is allowed" .omp/hooks/tests/mg64-policy.test.ts' } },
	{ name: "bash: commit message naming ctx.c is allowed", tool: "bash", input: { command: 'git add a.ts && git commit -q -m "harness: allow ctx.c excludes; keep reads blocked" -m "body"' } },
	{ name: "bash: commit message from ctx.c is blocked", tool: "bash", input: { command: 'git commit -m "$(cat ctx.c)"' }, blocked: CTX },
	{ name: "bash: committing ctx.c is blocked", tool: "bash", input: { command: "git add ctx.c && git commit -m x" }, blocked: CTX },
	{ name: "bash: grep -v temp.c is still blocked (temp.c is not exempt)", tool: "bash", input: { command: "grep -rn foo src | grep -v temp.c" }, blocked: TEMP },
	{ name: "bash: rm temp.c is blocked", tool: "bash", input: { command: "rm temp.c" }, blocked: TEMP },
	{ name: "bash: cat asm file is allowed", tool: "bash", input: { command: "cat asm/foo.s" } },
	{ name: "bash: grep in asm is allowed", tool: "bash", input: { command: "grep -r 'jal' asm/" } },
	{ name: "bash: rm asm file is blocked", tool: "bash", input: { command: "rm asm/foo.s" }, blocked: ASM_BASH },
	{ name: "bash: redirect into asm is blocked", tool: "bash", input: { command: "echo x > asm/foo.s" }, blocked: ASM_BASH },
	{
		name: "bash: sed -i on asm is blocked",
		tool: "bash",
		input: { command: "sed -i 's/a/b/' asm/foo.s" },
		blocked: ASM_BASH,
	},
	{
		name: "bash: git checkout of asm is blocked",
		tool: "bash",
		input: { command: "git checkout -- asm/foo.s" },
		blocked: ASM_BASH,
	},
	{
		name: "bash: python write into asm is blocked",
		tool: "bash",
		input: { command: `python3 -c 'pathlib.Path("asm/x.s").write_text("")'` },
		blocked: ASM_BASH,
	},
	{
		name: "bash: cp into tools/kmc-cc is blocked",
		tool: "bash",
		input: { command: "cp src/a.c tools/kmc-cc" },
		blocked: VENDORED,
	},
	{ name: "bash: rm of tools/gcc_kmc is blocked", tool: "bash", input: { command: "rm -rf tools/gcc_kmc" }, blocked: VENDORED },
	{ name: "bash: cat of vendored tool is allowed", tool: "bash", input: { command: "cat tools/kmc-cc" } },
	{
		name: "bash: copy then run of kmc-cc in a later segment is allowed",
		tool: "bash",
		input: { command: "cp src/a.c /tmp/a.c && ./tools/kmc-cc -c /tmp/a.c -o /tmp/a.o" },
	},
	{
		name: "bash: copy then read of vendored tool in a later segment is allowed",
		tool: "bash",
		input: { command: "cp src/a.c /tmp/a.c; cat tools/kmc-cc" },
	},
	{
		name: "bash: cp into vendored tool in a later segment is blocked",
		tool: "bash",
		input: { command: "cd /tmp && cp src/a.c tools/kmc-cc" },
		blocked: VENDORED,
	},
	{
		name: "bash: cp with fd duplication before a vendored target is blocked",
		tool: "bash",
		input: { command: "cp src/a.c 2>&1 tools/kmc-cc" },
		blocked: VENDORED,
	},
	{
		name: "bash: stderr redirect before a vendored argument is allowed",
		tool: "bash",
		input: { command: "ls 2>/dev/null tools/kmc-cc" },
	},
	{
		name: "bash: /dev/null redirect does not make a later vendored read a write",
		tool: "bash",
		input: { command: "cat src/a.c 2>/dev/null; cat tools/kmc-cc" },
	},
	{
		name: "bash: fd duplication before a pipe into grep is allowed",
		tool: "bash",
		input: { command: "ls 2>&1 | grep tools/gcc_kmc" },
	},
	{
		name: "bash: vendored file read with stderr to /dev/null is allowed",
		tool: "bash",
		input: { command: "grep -n foo tools/gcc_kmc/x.c 2>/dev/null" },
	},
	{ name: "bash: redirect into tools/kmc-cc is blocked", tool: "bash", input: { command: "echo x > tools/kmc-cc" }, blocked: VENDORED },
	{ name: "bash: attached append into tools/gcc_kmc is blocked", tool: "bash", input: { command: "echo x >>tools/gcc_kmc/x.c" }, blocked: VENDORED },
	{
		name: "bash: redirect after a benign redirect into tools/asm-differ is blocked",
		tool: "bash",
		input: { command: "echo x 2>/dev/null > tools/asm-differ/x.py" },
		blocked: VENDORED,
	},
	{
		name: "bash: quoted redirect target into tools/decomp-permuter is blocked",
		tool: "bash",
		input: { command: "echo x > \"tools/decomp-permuter/permuter.py\"" },
		blocked: VENDORED,
	},
	{
		name: "bash: redirect target built by command substitution is blocked",
		tool: "bash",
		input: { command: "echo x > $(echo tools/kmc-cc)" },
		blocked: VENDORED,
	},
	{ name: "bash: bare permuter is blocked", tool: "bash", input: { command: "./tools/permuter foo" }, blocked: NO_TIMEOUT },
	{
		name: "bash: timeout-wrapped permuter is allowed",
		tool: "bash",
		input: { command: "timeout 300s ./tools/permuter --source-file x" },
	},
	{
		name: "bash: permuter after ; without timeout is blocked",
		tool: "bash",
		input: { command: "timeout 300s true; ./tools/permuter x" },
		blocked: NO_TIMEOUT,
	},
	{
		name: "bash: permuter after & without timeout is blocked",
		tool: "bash",
		input: { command: "timeout 300s ./tools/permuter x & ./tools/permuter y" },
		blocked: NO_TIMEOUT,
	},
	{
		name: "bash: permuter.py after && without timeout is blocked",
		tool: "bash",
		input: { command: "timeout 300s ./tools/permuter a && ./tools/decomp-permuter/permuter.py b" },
		blocked: NO_TIMEOUT,
	},
	{
		name: "bash: env assignment before timeout is allowed",
		tool: "bash",
		input: { command: "FOO=1 timeout 300 ./tools/permuter x" },
	},
	{
		name: "bash: timeout flags are allowed",
		tool: "bash",
		input: { command: "timeout --foreground -k 5s -s TERM 300s ./tools/permuter x" },
	},
	{ name: "bash: minute duration is allowed", tool: "bash", input: { command: "timeout 1.5m ./tools/permuter x" } },
	{ name: "bash: zero duration is blocked", tool: "bash", input: { command: "timeout 0 ./tools/permuter x" }, blocked: NO_TIMEOUT },
	{
		name: "bash: unterminated quote is blocked",
		tool: "bash",
		input: { command: 'echo "hi ./tools/permuter' },
		blocked: PARSE,
	},
	{
		name: "bash: trailing backslash on a permuter command is blocked",
		tool: "bash",
		input: { command: "./tools/permuter x\\" },
		blocked: PARSE,
	},
	{
		name: "bash: unbalanced apostrophe without permuter is allowed",
		tool: "bash",
		input: { command: "cat <<'EOF'\ndon't stop\nEOF" },
	},
	{ name: "bash: reading the permuter wrapper is allowed", tool: "bash", input: { command: "sed -n '2,6p' tools/permuter" } },
	{
		name: "bash: reading permuter.py is allowed",
		tool: "bash",
		input: { command: "cat tools/decomp-permuter/permuter.py | grep -n timeout" },
	},
	{
		name: "bash: git log of the permuter wrapper is allowed",
		tool: "bash",
		input: { command: "git log --oneline -- tools/permuter" },
	},
	{
		name: "bash: permuter run after a read is still blocked",
		tool: "bash",
		input: { command: "cat tools/permuter && ./tools/permuter x" },
		blocked: NO_TIMEOUT,
	},
	{
		name: "bash: interpreter-launched permuter without timeout is blocked",
		tool: "bash",
		input: { command: "python3 tools/decomp-permuter/permuter.py x" },
		blocked: NO_TIMEOUT,
	},
	{ name: "bash: ls is allowed", tool: "bash", input: { command: "ls -la" } },
	{ name: "bash: empty command is allowed", tool: "bash", input: { command: "" } },
	{ name: "bash: missing command is allowed", tool: "bash", input: {} },

	// unrelated tools
	{ name: "grep: ctx.c pattern is not policed", tool: "grep", input: { pattern: "ctx.c" } },
];

describe("mg64-policy decisions", () => {
	for (const c of cases) {
		test(c.name, async () => {
			const reason = await decide(c.tool, c.input, c.cwd);
			if (c.blocked === undefined) {
				expect(reason).toBeUndefined();
			} else {
				expect(reason).toBeDefined();
				expect(reason).toContain(c.blocked);
			}
		});
	}
});

describe("permuter deadline", () => {
	const saved = process.env.NIGEL_TIMEOUT_DEADLINE_UNIX;
	const now = () => Math.trunc(Date.now() / 1000);
	const permuter = "timeout 300s ./tools/permuter x";

	beforeEach(() => {
		delete process.env.NIGEL_TIMEOUT_DEADLINE_UNIX;
	});
	afterEach(() => {
		if (saved === undefined) delete process.env.NIGEL_TIMEOUT_DEADLINE_UNIX;
		else process.env.NIGEL_TIMEOUT_DEADLINE_UNIX = saved;
	});

	test("empty deadline is ignored", async () => {
		process.env.NIGEL_TIMEOUT_DEADLINE_UNIX = "";
		expect(await decide("bash", { command: permuter })).toBeUndefined();
	});

	test("run that ends well before the deadline is allowed", async () => {
		process.env.NIGEL_TIMEOUT_DEADLINE_UNIX = String(now() + 1000);
		expect(await decide("bash", { command: permuter })).toBeUndefined();
	});

	test("run that ends inside the 120s reserve is blocked with skip hint", async () => {
		process.env.NIGEL_TIMEOUT_DEADLINE_UNIX = String(now() + 100);
		const reason = await decide("bash", { command: permuter });
		expect(reason).toContain("would leave less than 120 seconds");
		expect(reason).toContain("Skip the run.");
	});

	test("run that ends inside the reserve reports the largest safe timeout", async () => {
		process.env.NIGEL_TIMEOUT_DEADLINE_UNIX = String(now() + 400);
		const reason = await decide("bash", { command: permuter });
		expect(reason).toContain("Use timeout no greater than");
	});

	test("non-integer deadline is blocked", async () => {
		process.env.NIGEL_TIMEOUT_DEADLINE_UNIX = "soon";
		expect(await decide("bash", { command: permuter })).toContain(
			"not a valid Unix timestamp",
		);
	});
});
