import {
    createConnection,
    TextDocuments,
    ProposedFeatures,
    InitializeParams,
    TextDocumentSyncKind,
    InitializeResult,
    CompletionItem,
    CompletionItemKind,
    TextDocumentPositionParams,
    DidChangeConfigurationParams,
    PublishDiagnosticsParams,
    Diagnostic,
    DiagnosticSeverity,
} from 'vscode-languageserver/node';
import { TextDocument } from 'vscode-languageserver-textdocument';
import { execFile } from 'child_process';
import { promisify } from 'util';
import * as path from 'path';
import * as fs from 'fs';

const execFileAsync = promisify(execFile);

const connection = createConnection(ProposedFeatures.all);
const documents = new TextDocuments(TextDocument);

// Built-in Flow components and keywords
const BUILTINS = [
    { label: 'col', kind: CompletionItemKind.Class, detail: 'Layout column' },
    { label: 'row', kind: CompletionItemKind.Class, detail: 'Layout row' },
    { label: 'text', kind: CompletionItemKind.Class, detail: 'Text element' },
    { label: 'button', kind: CompletionItemKind.Class, detail: 'Button' },
    { label: 'input', kind: CompletionItemKind.Class, detail: 'Input field' },
    { label: 'img', kind: CompletionItemKind.Class, detail: 'Image' },
    { label: 'link', kind: CompletionItemKind.Class, detail: 'Link' },
    { label: 'show', kind: CompletionItemKind.Keyword, detail: 'Conditional render' },
    { label: 'switch', kind: CompletionItemKind.Keyword, detail: 'Switch/match' },
    { label: 'match', kind: CompletionItemKind.Keyword, detail: 'Pattern match' },
    { label: 'each', kind: CompletionItemKind.Keyword, detail: 'List iteration' },
    { label: 'App', kind: CompletionItemKind.Class, detail: 'Root app component' },
];

const ON_ATTRS = [
    { label: 'on:click', kind: CompletionItemKind.Method, detail: 'Click handler, e.g. on:click={server.ping}' },
    { label: 'on:change', kind: CompletionItemKind.Method, detail: 'Change handler' },
    { label: 'on:blur', kind: CompletionItemKind.Method, detail: 'Blur handler' },
    { label: 'on:focus', kind: CompletionItemKind.Method, detail: 'Focus handler' },
];

const KEYWORDS = [
    { label: 'default', kind: CompletionItemKind.Keyword },
    { label: 'export', kind: CompletionItemKind.Keyword },
    { label: 'fn', kind: CompletionItemKind.Keyword },
    { label: 'input', kind: CompletionItemKind.Keyword },
    { label: 'return', kind: CompletionItemKind.Keyword },
];

let flowcPath: string | null = null;
let workspaceRoot: string | null = null;

function findFlowc(): string | null {
    if (flowcPath) return flowcPath;
    const roots = workspaceRoot ? [workspaceRoot] : [];
    const root = workspaceRoot || '';
    const candidates = [
        path.join(root, 'compiler', 'flowc.exe'),
        path.join(root, 'compiler', 'flowc'),
        path.join(root, 'out', 'flowc.exe'),
        path.join(root, 'out', 'flowc'),
        path.join(root, 'flowc.exe'),
        path.join(root, 'flowc'),
        'flowc',
    ];
    for (const c of candidates) {
        try {
            if (fs.existsSync(c)) return c;
        } catch { /* ignore */ }
    }
    return 'flowc';
}

function uriToFsPath(uri: string): string {
    if (!uri.startsWith('file:')) return uri;
    let p = decodeURIComponent(uri.slice(7));
    if (p.charAt(0) === '/' && /^\/[A-Za-z]:/i.test(p)) p = p.slice(1);
    return p;
}

async function getServerFunctions(uri: string): Promise<string[]> {
    const docPath = path.dirname(uriToFsPath(uri));
    const maxDepth = 10;
    const roots = [docPath];
    if (workspaceRoot) {
        roots.push(workspaceRoot, path.join(workspaceRoot, 'dev'));
    }
    for (const root of roots) {
        let d = root;
        for (let i = 0; i < maxDepth; i++) {
            const serverDir = path.join(d, 'server', 'functions');
            if (fs.existsSync(serverDir)) {
            const names: string[] = [];
                const files = fs.readdirSync(serverDir).filter(f => f.endsWith('.flow'));
                for (const f of files) {
                    const content = fs.readFileSync(path.join(serverDir, f), 'utf-8');
                    const re = /export\s+(\w+)\s*=/g;
                    let m;
                    while ((m = re.exec(content)) !== null) names.push(m[1]);
                }
                return names;
            }
            const parent = path.dirname(d);
            if (parent === d) break;
            d = parent;
        }
    }
    return [];
}

connection.onInitialize((params: InitializeParams): InitializeResult => {
    workspaceRoot = params.rootUri ? uriToFsPath(params.rootUri) : null;
    return {
        capabilities: {
            textDocumentSync: TextDocumentSyncKind.Incremental,
            completionProvider: { triggerCharacters: ['.', ':', '{', '<', ' '] },
            hoverProvider: true,
        },
    };
});

connection.onDidChangeConfiguration((_params: DidChangeConfigurationParams) => {
    flowcPath = null;
});

documents.onDidChangeContent(async (change) => {
    await validate(change.document);
});

documents.onDidOpen(async (event) => {
    await validate(event.document);
});

async function validate(document: TextDocument) {
    const uri = document.uri;
    const flowc: string = findFlowc() ?? 'flowc';
    const filePath = uriToFsPath(uri);
    let diagnostics: Diagnostic[] = [];

    try {
        const { stdout, stderr } = await execFileAsync(flowc, ['--check', filePath], {
            timeout: 5000,
            encoding: 'utf-8',
            maxBuffer: 1024 * 1024,
        });
        const output = stdout.trim() || stderr.trim();
        if (output) {
            try {
                const arr = JSON.parse(output);

                if (Array.isArray(arr)) {
                    for (const e of arr) {
                        if (e.line != null && e.message) {
                            diagnostics.push({
                                range: {
                                    start: { line: Math.max(0, (e.line || 1) - 1), character: e.col || 0 },
                                    end: { line: Math.max(0, (e.line || 1) - 1), character: (e.col || 0) + 1 },
                                },
                                message: String(e.message),
                                severity: DiagnosticSeverity.Error,
                                source: 'flowc',
                            });
                        }
                    }
                }
            } catch {
                // Non-JSON output: treat as generic error
                const lines = output.split('\n');
                for (let i = 0; i < lines.length; i++) {
                    if (lines[i].trim()) {
                        diagnostics.push({
                            range: {
                                start: { line: i, character: 0 },
                                end: { line: i, character: 1 },
                            },
                            message: lines[i].trim(),
                            severity: DiagnosticSeverity.Error,
                            source: 'flowc',
                        });
                    }
                }
            }
        }
    } catch (err: unknown) {
        // flowc not found or failed — don't spam diagnostics
        const msg = err instanceof Error ? err.message : String(err);
        if (!msg.includes('ENOENT') && !msg.includes('not found')) {
            connection.console.log(`flowc --check failed: ${msg}`);
        }
    }

    connection.sendDiagnostics({ uri, diagnostics });
}

connection.onCompletion(async (params: TextDocumentPositionParams): Promise<CompletionItem[]> => {
    const doc = documents.get(params.textDocument.uri);
    if (!doc) return [];

    const text = doc.getText();
    const offset = doc.offsetAt(params.position);
    const line = doc.getText({ start: { line: params.position.line, character: 0 }, end: params.position });
    const beforeCursor = line.slice(0, params.position.character);

    const items: CompletionItem[] = [];

    // server.xxx — anche dentro {server.} o server.p
    const inServerContext = /\bserver\.\w*$/.test(beforeCursor);
    if (inServerContext) {
        const fns = await getServerFunctions(params.textDocument.uri);
        for (const fn of fns) {
            items.push({
                label: fn,
                kind: CompletionItemKind.Method,
                detail: `server.${fn}`,
                insertText: fn,
            });
        }
    }

    // on:click, on:change — quando si digita "on" o "on:" (evita che "button" matchi "on" e mostri icona sbagliata)
    const inOnAttrContext = /on:?\w*$/.test(beforeCursor) && /[<\s]on:?\w*$/.test(beforeCursor);
    if (inOnAttrContext) {
        for (const a of ON_ATTRS) items.push({ ...a });
    }

    // Inside JSX <...> — suggest builtins solo se non siamo in contesto attributo "on"
    const openIdx = text.lastIndexOf('<', offset);
    const closeIdx = text.indexOf('>', offset);
    const insideTag = openIdx >= 0 && (closeIdx < 0 || closeIdx > offset);
    const tagContent = insideTag ? text.slice(openIdx, offset) : '';
    const typingTagName = insideTag && !tagContent.includes(' ') && !tagContent.includes('>') && !tagContent.includes('/');
    if (typingTagName && !inOnAttrContext) {
        for (const b of BUILTINS) items.push({ ...b });
    }

    // Keywords at top level
    if (/^\s*(default|export)\s+\w*$/.test(beforeCursor) || /^\s*\w*$/.test(beforeCursor.trim())) {
        for (const k of KEYWORDS) items.push({ ...k });
    }

    if (items.length === 0) {
        if (!inOnAttrContext) for (const b of BUILTINS) items.push({ ...b });
        for (const a of ON_ATTRS) items.push({ ...a });
    }

    return items;
});

connection.onHover((params) => {
    const doc = documents.get(params.textDocument.uri);
    if (!doc) return null;

    const text = doc.getText();
    const offset = doc.offsetAt(params.position);
    const line = doc.getText({ start: { line: params.position.line, character: 0 }, end: params.position });

    const wordMatch = line.match(/(\w+)$/);
    const word = wordMatch ? wordMatch[1] : '';

    for (const b of BUILTINS) {
        if (b.label === word) return { contents: { kind: 'markdown', value: `**${b.label}** — ${b.detail || 'Flow built-in'}` } };
    }
    for (const a of ON_ATTRS) {
        if (a.label.startsWith('on:') && line.includes(a.label)) return { contents: { kind: 'markdown', value: `**${a.label}** — ${a.detail || ''}` } };
    }

    return null;
});

documents.listen(connection);
connection.listen();
