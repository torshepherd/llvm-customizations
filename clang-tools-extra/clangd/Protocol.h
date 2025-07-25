//===--- Protocol.h - Language Server Protocol Implementation ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains structs based on the LSP specification at
// https://github.com/Microsoft/language-server-protocol/blob/main/protocol.md
//
// This is not meant to be a complete implementation, new interfaces are added
// when they're needed.
//
// Each struct has a toJSON and fromJSON function, that converts between
// the struct and a JSON representation. (See JSON.h)
//
// Some structs also have operator<< serialization. This is for debugging and
// tests, and is not generally machine-readable.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANGD_PROTOCOL_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANGD_PROTOCOL_H

#include "URI.h"
#include "index/SymbolID.h"
#include "support/MemoryTree.h"
#include "clang/Index/IndexSymbol.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"
#include <bitset>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// This file is using the LSP syntax for identifier names which is different
// from the LLVM coding standard. To avoid the clang-tidy warnings, we're
// disabling one check here.
// NOLINTBEGIN(readability-identifier-naming)

namespace clang {
namespace clangd {

enum class ErrorCode {
  // Defined by JSON RPC.
  ParseError = -32700,
  InvalidRequest = -32600,
  MethodNotFound = -32601,
  InvalidParams = -32602,
  InternalError = -32603,

  ServerNotInitialized = -32002,
  UnknownErrorCode = -32001,

  // Defined by the protocol.
  RequestCancelled = -32800,
  ContentModified = -32801,
};
// Models an LSP error as an llvm::Error.
class LSPError : public llvm::ErrorInfo<LSPError> {
public:
  std::string Message;
  ErrorCode Code;
  static char ID;

  LSPError(std::string Message, ErrorCode Code)
      : Message(std::move(Message)), Code(Code) {}

  void log(llvm::raw_ostream &OS) const override {
    OS << int(Code) << ": " << Message;
  }
  std::error_code convertToErrorCode() const override {
    return llvm::inconvertibleErrorCode();
  }
};

bool fromJSON(const llvm::json::Value &, SymbolID &, llvm::json::Path);
llvm::json::Value toJSON(const SymbolID &);

// URI in "file" scheme for a file.
struct URIForFile {
  URIForFile() = default;

  /// Canonicalizes \p AbsPath via URI.
  ///
  /// File paths in URIForFile can come from index or local AST. Path from
  /// index goes through URI transformation, and the final path is resolved by
  /// URI scheme and could potentially be different from the original path.
  /// Hence, we do the same transformation for all paths.
  ///
  /// Files can be referred to by several paths (e.g. in the presence of links).
  /// Which one we prefer may depend on where we're coming from. \p TUPath is a
  /// hint, and should usually be the main entrypoint file we're processing.
  static URIForFile canonicalize(llvm::StringRef AbsPath,
                                 llvm::StringRef TUPath);

  static llvm::Expected<URIForFile> fromURI(const URI &U,
                                            llvm::StringRef HintPath);

  /// Retrieves absolute path to the file.
  llvm::StringRef file() const { return File; }

  explicit operator bool() const { return !File.empty(); }
  std::string uri() const { return URI::createFile(File).toString(); }

  friend bool operator==(const URIForFile &LHS, const URIForFile &RHS) {
    return LHS.File == RHS.File;
  }

  friend bool operator!=(const URIForFile &LHS, const URIForFile &RHS) {
    return !(LHS == RHS);
  }

  friend bool operator<(const URIForFile &LHS, const URIForFile &RHS) {
    return LHS.File < RHS.File;
  }

private:
  explicit URIForFile(std::string &&File) : File(std::move(File)) {}

  std::string File;
};

/// Serialize/deserialize \p URIForFile to/from a string URI.
llvm::json::Value toJSON(const URIForFile &U);
bool fromJSON(const llvm::json::Value &, URIForFile &, llvm::json::Path);

struct TextDocumentIdentifier {
  /// The text document's URI.
  URIForFile uri;
};
llvm::json::Value toJSON(const TextDocumentIdentifier &);
bool fromJSON(const llvm::json::Value &, TextDocumentIdentifier &,
              llvm::json::Path);

struct VersionedTextDocumentIdentifier : public TextDocumentIdentifier {
  /// The version number of this document. If a versioned text document
  /// identifier is sent from the server to the client and the file is not open
  /// in the editor (the server has not received an open notification before)
  /// the server can send `null` to indicate that the version is known and the
  /// content on disk is the master (as speced with document content ownership).
  ///
  /// The version number of a document will increase after each change,
  /// including undo/redo. The number doesn't need to be consecutive.
  ///
  /// clangd extension: versions are optional, and synthesized if missing.
  std::optional<std::int64_t> version;
};
llvm::json::Value toJSON(const VersionedTextDocumentIdentifier &);
bool fromJSON(const llvm::json::Value &, VersionedTextDocumentIdentifier &,
              llvm::json::Path);

struct Position {
  /// Line position in a document (zero-based).
  int line = 0;

  /// Character offset on a line in a document (zero-based).
  /// WARNING: this is in UTF-16 codepoints, not bytes or characters!
  /// Use the functions in SourceCode.h to construct/interpret Positions.
  int character = 0;

  friend bool operator==(const Position &LHS, const Position &RHS) {
    return std::tie(LHS.line, LHS.character) ==
           std::tie(RHS.line, RHS.character);
  }
  friend bool operator!=(const Position &LHS, const Position &RHS) {
    return !(LHS == RHS);
  }
  friend bool operator<(const Position &LHS, const Position &RHS) {
    return std::tie(LHS.line, LHS.character) <
           std::tie(RHS.line, RHS.character);
  }
  friend bool operator<=(const Position &LHS, const Position &RHS) {
    return std::tie(LHS.line, LHS.character) <=
           std::tie(RHS.line, RHS.character);
  }
};
bool fromJSON(const llvm::json::Value &, Position &, llvm::json::Path);
llvm::json::Value toJSON(const Position &);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, const Position &);

struct Range {
  /// The range's start position.
  Position start;

  /// The range's end position.
  Position end;

  friend bool operator==(const Range &LHS, const Range &RHS) {
    return std::tie(LHS.start, LHS.end) == std::tie(RHS.start, RHS.end);
  }
  friend bool operator!=(const Range &LHS, const Range &RHS) {
    return !(LHS == RHS);
  }
  friend bool operator<(const Range &LHS, const Range &RHS) {
    return std::tie(LHS.start, LHS.end) < std::tie(RHS.start, RHS.end);
  }

  bool contains(Position Pos) const { return start <= Pos && Pos < end; }
  bool contains(Range Rng) const {
    return start <= Rng.start && Rng.end <= end;
  }
};
bool fromJSON(const llvm::json::Value &, Range &, llvm::json::Path);
llvm::json::Value toJSON(const Range &);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, const Range &);

struct Location {
  /// The text document's URI.
  URIForFile uri;
  Range range;

  friend bool operator==(const Location &LHS, const Location &RHS) {
    return LHS.uri == RHS.uri && LHS.range == RHS.range;
  }

  friend bool operator!=(const Location &LHS, const Location &RHS) {
    return !(LHS == RHS);
  }

  friend bool operator<(const Location &LHS, const Location &RHS) {
    return std::tie(LHS.uri, LHS.range) < std::tie(RHS.uri, RHS.range);
  }
};
llvm::json::Value toJSON(const Location &);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, const Location &);

/// Extends Locations returned by textDocument/references with extra info.
/// This is a clangd extension: LSP uses `Location`.
struct ReferenceLocation : Location {
  /// clangd extension: contains the name of the function or class in which the
  /// reference occurs
  std::optional<std::string> containerName;
};
llvm::json::Value toJSON(const ReferenceLocation &);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, const ReferenceLocation &);

using ChangeAnnotationIdentifier = std::string;
// A combination of a LSP standard TextEdit and AnnotatedTextEdit.
struct TextEdit {
  /// The range of the text document to be manipulated. To insert
  /// text into a document create a range where start === end.
  Range range;

  /// The string to be inserted. For delete operations use an
  /// empty string.
  std::string newText;

  /// The actual annotation identifier (optional)
  /// If empty, then this field is nullopt.
  ChangeAnnotationIdentifier annotationId = "";
};
inline bool operator==(const TextEdit &L, const TextEdit &R) {
  return std::tie(L.newText, L.range, L.annotationId) ==
         std::tie(R.newText, R.range, L.annotationId);
}
inline bool operator<(const TextEdit &L, const TextEdit &R) {
  return std::tie(L.newText, L.range, L.annotationId) <
         std::tie(R.newText, R.range, L.annotationId);
}
bool fromJSON(const llvm::json::Value &, TextEdit &, llvm::json::Path);
llvm::json::Value toJSON(const TextEdit &);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, const TextEdit &);

struct ChangeAnnotation {
  /// A human-readable string describing the actual change. The string
  /// is rendered prominent in the user interface.
  std::string label;

  /// A flag which indicates that user confirmation is needed
  /// before applying the change.
  std::optional<bool> needsConfirmation;

  /// A human-readable string which is rendered less prominent in
  /// the user interface.
  std::string description;
};
bool fromJSON(const llvm::json::Value &, ChangeAnnotation &, llvm::json::Path);
llvm::json::Value toJSON(const ChangeAnnotation &);

struct TextDocumentEdit {
  /// The text document to change.
  VersionedTextDocumentIdentifier textDocument;

  /// The edits to be applied.
  /// FIXME: support the AnnotatedTextEdit variant.
  std::vector<TextEdit> edits;
};
bool fromJSON(const llvm::json::Value &, TextDocumentEdit &, llvm::json::Path);
llvm::json::Value toJSON(const TextDocumentEdit &);

struct TextDocumentItem {
  /// The text document's URI.
  URIForFile uri;

  /// The text document's language identifier.
  std::string languageId;

  /// The version number of this document (it will strictly increase after each
  /// change, including undo/redo.
  ///
  /// clangd extension: versions are optional, and synthesized if missing.
  std::optional<int64_t> version;

  /// The content of the opened text document.
  std::string text;
};
bool fromJSON(const llvm::json::Value &, TextDocumentItem &, llvm::json::Path);

enum class TraceLevel {
  Off = 0,
  Messages = 1,
  Verbose = 2,
};
bool fromJSON(const llvm::json::Value &E, TraceLevel &Out, llvm::json::Path);

struct NoParams {};
inline llvm::json::Value toJSON(const NoParams &) { return nullptr; }
inline bool fromJSON(const llvm::json::Value &, NoParams &, llvm::json::Path) {
  return true;
}
using InitializedParams = NoParams;

/// Defines how the host (editor) should sync document changes to the language
/// server.
enum class TextDocumentSyncKind {
  /// Documents should not be synced at all.
  None = 0,

  /// Documents are synced by always sending the full content of the document.
  Full = 1,

  /// Documents are synced by sending the full content on open.  After that
  /// only incremental updates to the document are send.
  Incremental = 2,
};

/// The kind of a completion entry.
enum class CompletionItemKind {
  Missing = 0,
  Text = 1,
  Method = 2,
  Function = 3,
  Constructor = 4,
  Field = 5,
  Variable = 6,
  Class = 7,
  Interface = 8,
  Module = 9,
  Property = 10,
  Unit = 11,
  Value = 12,
  Enum = 13,
  Keyword = 14,
  Snippet = 15,
  Color = 16,
  File = 17,
  Reference = 18,
  Folder = 19,
  EnumMember = 20,
  Constant = 21,
  Struct = 22,
  Event = 23,
  Operator = 24,
  TypeParameter = 25,
};
bool fromJSON(const llvm::json::Value &, CompletionItemKind &,
              llvm::json::Path);
constexpr auto CompletionItemKindMin =
    static_cast<size_t>(CompletionItemKind::Text);
constexpr auto CompletionItemKindMax =
    static_cast<size_t>(CompletionItemKind::TypeParameter);
using CompletionItemKindBitset = std::bitset<CompletionItemKindMax + 1>;
bool fromJSON(const llvm::json::Value &, CompletionItemKindBitset &,
              llvm::json::Path);
CompletionItemKind
adjustKindToCapability(CompletionItemKind Kind,
                       CompletionItemKindBitset &SupportedCompletionItemKinds);

/// A symbol kind.
enum class SymbolKind {
  File = 1,
  Module = 2,
  Namespace = 3,
  Package = 4,
  Class = 5,
  Method = 6,
  Property = 7,
  Field = 8,
  Constructor = 9,
  Enum = 10,
  Interface = 11,
  Function = 12,
  Variable = 13,
  Constant = 14,
  String = 15,
  Number = 16,
  Boolean = 17,
  Array = 18,
  Object = 19,
  Key = 20,
  Null = 21,
  EnumMember = 22,
  Struct = 23,
  Event = 24,
  Operator = 25,
  TypeParameter = 26
};
bool fromJSON(const llvm::json::Value &, SymbolKind &, llvm::json::Path);
constexpr auto SymbolKindMin = static_cast<size_t>(SymbolKind::File);
constexpr auto SymbolKindMax = static_cast<size_t>(SymbolKind::TypeParameter);
using SymbolKindBitset = std::bitset<SymbolKindMax + 1>;
bool fromJSON(const llvm::json::Value &, SymbolKindBitset &, llvm::json::Path);
SymbolKind adjustKindToCapability(SymbolKind Kind,
                                  SymbolKindBitset &supportedSymbolKinds);

// Convert a index::SymbolKind to clangd::SymbolKind (LSP)
// Note, some are not perfect matches and should be improved when this LSP
// issue is addressed:
// https://github.com/Microsoft/language-server-protocol/issues/344
SymbolKind indexSymbolKindToSymbolKind(index::SymbolKind Kind);

// Determines the encoding used to measure offsets and lengths of source in LSP.
enum class OffsetEncoding {
  // Any string is legal on the wire. Unrecognized encodings parse as this.
  UnsupportedEncoding,
  // Length counts code units of UTF-16 encoded text. (Standard LSP behavior).
  UTF16,
  // Length counts bytes of UTF-8 encoded text. (Clangd extension).
  UTF8,
  // Length counts codepoints in unicode text. (Clangd extension).
  UTF32,
};
llvm::json::Value toJSON(const OffsetEncoding &);
bool fromJSON(const llvm::json::Value &, OffsetEncoding &, llvm::json::Path);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, OffsetEncoding);

// Describes the content type that a client supports in various result literals
// like `Hover`, `ParameterInfo` or `CompletionItem`.
enum class MarkupKind {
  PlainText,
  Markdown,
};
bool fromJSON(const llvm::json::Value &, MarkupKind &, llvm::json::Path);
llvm::raw_ostream &operator<<(llvm::raw_ostream &OS, MarkupKind);

// This struct doesn't mirror LSP!
// The protocol defines deeply nested structures for client capabilities.
// Instead of mapping them all, this just parses out the bits we care about.
struct ClientCapabilities {
  /// The supported set of SymbolKinds for workspace/symbol.
  /// workspace.symbol.symbolKind.valueSet
  std::optional<SymbolKindBitset> WorkspaceSymbolKinds;

  /// Whether the client accepts diagnostics with codeActions attached inline.
  /// This is a clangd extension.
  /// textDocument.publishDiagnostics.codeActionsInline.
  bool DiagnosticFixes = false;

  /// Whether the client accepts diagnostics with related locations.
  /// textDocument.publishDiagnostics.relatedInformation.
  bool DiagnosticRelatedInformation = false;

  /// Whether the client accepts diagnostics with category attached to it
  /// using the "category" extension.
  /// textDocument.publishDiagnostics.categorySupport
  bool DiagnosticCategory = false;

  /// Client supports snippets as insert text.
  /// textDocument.completion.completionItem.snippetSupport
  bool CompletionSnippets = false;

  /// Client supports completions with additionalTextEdit near the cursor.
  /// This is a clangd extension. (LSP says this is for unrelated text only).
  /// textDocument.completion.editsNearCursor
  bool CompletionFixes = false;

  /// Client supports displaying a container string for results of
  /// textDocument/reference (clangd extension)
  /// textDocument.references.container
  bool ReferenceContainer = false;

  /// Client supports hierarchical document symbols.
  /// textDocument.documentSymbol.hierarchicalDocumentSymbolSupport
  bool HierarchicalDocumentSymbol = false;

  /// Client supports signature help.
  /// textDocument.signatureHelp
  bool HasSignatureHelp = false;

  /// Client signals that it only supports folding complete lines.
  /// Client will ignore specified `startCharacter` and `endCharacter`
  /// properties in a FoldingRange.
  /// textDocument.foldingRange.lineFoldingOnly
  bool LineFoldingOnly = false;

  /// Client supports processing label offsets instead of a simple label string.
  /// textDocument.signatureHelp.signatureInformation.parameterInformation.labelOffsetSupport
  bool OffsetsInSignatureHelp = false;

  /// The documentation format that should be used for
  /// textDocument/signatureHelp.
  /// textDocument.signatureHelp.signatureInformation.documentationFormat
  MarkupKind SignatureHelpDocumentationFormat = MarkupKind::PlainText;

  /// The supported set of CompletionItemKinds for textDocument/completion.
  /// textDocument.completion.completionItemKind.valueSet
  std::optional<CompletionItemKindBitset> CompletionItemKinds;

  /// The documentation format that should be used for textDocument/completion.
  /// textDocument.completion.completionItem.documentationFormat
  MarkupKind CompletionDocumentationFormat = MarkupKind::PlainText;

  /// The client has support for completion item label details.
  /// textDocument.completion.completionItem.labelDetailsSupport.
  bool CompletionLabelDetail = false;

  /// Client supports CodeAction return value for textDocument/codeAction.
  /// textDocument.codeAction.codeActionLiteralSupport.
  bool CodeActionStructure = false;

  /// Client advertises support for the semanticTokens feature.
  /// We support the textDocument/semanticTokens request in any case.
  /// textDocument.semanticTokens
  bool SemanticTokens = false;
  /// Client supports Theia semantic highlighting extension.
  /// https://github.com/microsoft/vscode-languageserver-node/pull/367
  /// clangd no longer supports this, we detect it just to log a warning.
  /// textDocument.semanticHighlightingCapabilities.semanticHighlighting
  bool TheiaSemanticHighlighting = false;

  /// Supported encodings for LSP character offsets. (clangd extension).
  std::optional<std::vector<OffsetEncoding>> offsetEncoding;

  /// The content format that should be used for Hover requests.
  /// textDocument.hover.contentEncoding
  MarkupKind HoverContentFormat = MarkupKind::PlainText;

  /// The client supports testing for validity of rename operations
  /// before execution.
  bool RenamePrepareSupport = false;

  /// The client supports progress notifications.
  /// window.workDoneProgress
  bool WorkDoneProgress = false;

  /// The client supports implicit $/progress work-done progress streams,
  /// without a preceding window/workDoneProgress/create.
  /// This is a clangd extension.
  /// window.implicitWorkDoneProgressCreate
  bool ImplicitProgressCreation = false;

  /// Whether the client claims to cancel stale requests.
  /// general.staleRequestSupport.cancel
  bool CancelsStaleRequests = false;

  /// Whether the client implementation supports a refresh request sent from the
  /// server to the client.
  bool SemanticTokenRefreshSupport = false;

  /// The client supports versioned document changes for WorkspaceEdit.
  bool DocumentChanges = false;

  /// The client supports change annotations on text edits,
  bool ChangeAnnotation = false;

  /// Whether the client supports the textDocument/inactiveRegions
  /// notification. This is a clangd extension.
  /// textDocument.inactiveRegionsCapabilities.inactiveRegions
  bool InactiveRegions = false;
};
bool fromJSON(const llvm::json::Value &, ClientCapabilities &,
              llvm::json::Path);

/// Clangd extension that's used in the 'compilationDatabaseChanges' in
/// workspace/didChangeConfiguration to record updates to the in-memory
/// compilation database.
struct ClangdCompileCommand {
  std::string workingDirectory;
  std::vector<std::string> compilationCommand;
};
bool fromJSON(const llvm::json::Value &, ClangdCompileCommand &,
              llvm::json::Path);

/// Clangd extension: parameters configurable at any time, via the
/// `workspace/didChangeConfiguration` notification.
/// LSP defines this type as `any`.
struct ConfigurationSettings {
  // Changes to the in-memory compilation database.
  // The key of the map is a file name.
  std::map<std::string, ClangdCompileCommand> compilationDatabaseChanges;
};
bool fromJSON(const llvm::json::Value &, ConfigurationSettings &,
              llvm::json::Path);

/// Clangd extension: parameters configurable at `initialize` time.
/// LSP defines this type as `any`.
struct InitializationOptions {
  // What we can change through the didChangeConfiguration request, we can
  // also set through the initialize request (initializationOptions field).
  ConfigurationSettings ConfigSettings;

  std::optional<std::string> compilationDatabasePath;
  // Additional flags to be included in the "fallback command" used when
  // the compilation database doesn't describe an opened file.
  // The command used will be approximately `clang $FILE $fallbackFlags`.
  std::vector<std::string> fallbackFlags;

  /// Clients supports show file status for textDocument/clangd.fileStatus.
  bool FileStatus = false;
};
bool fromJSON(const llvm::json::Value &, InitializationOptions &,
              llvm::json::Path);

struct InitializeParams {
  /// The process Id of the parent process that started
  /// the server. Is null if the process has not been started by another
  /// process. If the parent process is not alive then the server should exit
  /// (see exit notification) its process.
  std::optional<int> processId;

  /// The rootPath of the workspace. Is null
  /// if no folder is open.
  ///
  /// @deprecated in favour of rootUri.
  std::optional<std::string> rootPath;

  /// The rootUri of the workspace. Is null if no
  /// folder is open. If both `rootPath` and `rootUri` are set
  /// `rootUri` wins.
  std::optional<URIForFile> rootUri;

  // User provided initialization options.
  // initializationOptions?: any;

  /// The capabilities provided by the client (editor or tool)
  ClientCapabilities capabilities;
  /// The same data as capabilities, but not parsed (to expose to modules).
  llvm::json::Object rawCapabilities;

  /// The initial trace setting. If omitted trace is disabled ('off').
  std::optional<TraceLevel> trace;

  /// User-provided initialization options.
  InitializationOptions initializationOptions;
};
bool fromJSON(const llvm::json::Value &, InitializeParams &, llvm::json::Path);

struct WorkDoneProgressCreateParams {
  /// The token to be used to report progress.
  llvm::json::Value token = nullptr;
};
llvm::json::Value toJSON(const WorkDoneProgressCreateParams &P);

template <typename T> struct ProgressParams {
  /// The progress token provided by the client or server.
  llvm::json::Value token = nullptr;

  /// The progress data.
  T value;
};
template <typename T> llvm::json::Value toJSON(const ProgressParams<T> &P) {
  return llvm::json::Object{{"token", P.token}, {"value", P.value}};
}
/// To start progress reporting a $/progress notification with the following
/// payload must be sent.
struct WorkDoneProgressBegin {
  /// Mandatory title of the progress operation. Used to briefly inform about
  /// the kind of operation being performed.
  ///
  /// Examples: "Indexing" or "Linking dependencies".
  std::string title;

  /// Controls if a cancel button should show to allow the user to cancel the
  /// long-running operation. Clients that don't support cancellation are
  /// allowed to ignore the setting.
  bool cancellable = false;

  /// Optional progress percentage to display (value 100 is considered 100%).
  /// If not provided infinite progress is assumed and clients are allowed
  /// to ignore the `percentage` value in subsequent in report notifications.
  ///
  /// The value should be steadily rising. Clients are free to ignore values
  /// that are not following this rule.
  ///
  /// Clangd implementation note: we only send nonzero percentages in
  /// the WorkProgressReport. 'true' here means percentages will be used.
  bool percentage = false;
};
llvm::json::Value toJSON(const WorkDoneProgressBegin &);

/// Reporting progress is done using the following payload.
struct WorkDoneProgressReport {
  /// Mandatory title of the progress operation. Used to briefly inform about
  /// the kind of operation being performed.
  ///
  /// Examples: "Indexing" or "Linking dependencies".
  std::string title;

  /// Controls enablement state of a cancel button. This property is only valid
  /// if a cancel button got requested in the `WorkDoneProgressStart` payload.
  ///
  /// Clients that don't support cancellation or don't support control
  /// the button's enablement state are allowed to ignore the setting.
  std::optional<bool> cancellable;

  /// Optional, more detailed associated progress message. Contains
  /// complementary information to the `title`.
  ///
  /// Examples: "3/25 files", "project/src/module2", "node_modules/some_dep".
  /// If unset, the previous progress message (if any) is still valid.
  std::optional<std::string> message;

  /// Optional progress percentage to display (value 100 is considered 100%).
  /// If not provided infinite progress is assumed and clients are allowed
  /// to ignore the `percentage` value in subsequent in report notifications.
  ///
  /// The value should be steadily rising. Clients are free to ignore values
  /// that are not following this rule.
  std::optional<unsigned> percentage;
};
llvm::json::Value toJSON(const WorkDoneProgressReport &);
//
/// Signals the end of progress reporting.
struct WorkDoneProgressEnd {
  /// Optional, a final message indicating to for example indicate the outcome
  /// of the operation.
  std::optional<std::string> message;
};
llvm::json::Value toJSON(const WorkDoneProgressEnd &);

enum class MessageType {
  /// An error message.
  Error = 1,
  /// A warning message.
  Warning = 2,
  /// An information message.
  Info = 3,
  /// A log message.
  Log = 4,
};
llvm::json::Value toJSON(const MessageType &);

/// The show message notification is sent from a server to a client to ask the
/// client to display a particular message in the user interface.
struct ShowMessageParams {
  /// The message type.
  MessageType type = MessageType::Info;
  /// The actual message.
  std::string message;
};
llvm::json::Value toJSON(const ShowMessageParams &);

struct DidOpenTextDocumentParams {
  /// The document that was opened.
  TextDocumentItem textDocument;
};
bool fromJSON(const llvm::json::Value &, DidOpenTextDocumentParams &,
              llvm::json::Path);

struct DidCloseTextDocumentParams {
  /// The document that was closed.
  TextDocumentIdentifier textDocument;
};
bool fromJSON(const llvm::json::Value &, DidCloseTextDocumentParams &,
              llvm::json::Path);

struct DidSaveTextDocumentParams {
  /// The document that was saved.
  TextDocumentIdentifier textDocument;
};
bool fromJSON(const llvm::json::Value &, DidSaveTextDocumentParams &,
              llvm::json::Path);

struct TextDocumentContentChangeEvent {
  /// The range of the document that changed.
  std::optional<Range> range;

  /// The length of the range that got replaced.
  std::optional<int> rangeLength;

  /// The new text of the range/document.
  std::string text;
};
bool fromJSON(const llvm::json::Value &, TextDocumentContentChangeEvent &,
              llvm::json::Path);

struct DidChangeTextDocumentParams {
  /// The document that did change. The version number points
  /// to the version after all provided content changes have
  /// been applied.
  VersionedTextDocumentIdentifier textDocument;

  /// The actual content changes.
  std::vector<TextDocumentContentChangeEvent> contentChanges;

  /// Forces diagnostics to be generated, or to not be generated, for this
  /// version of the file. If not set, diagnostics are eventually consistent:
  /// either they will be provided for this version or some subsequent one.
  /// This is a clangd extension.
  std::optional<bool> wantDiagnostics;

  /// Force a complete rebuild of the file, ignoring all cached state. Slow!
  /// This is useful to defeat clangd's assumption that missing headers will
  /// stay missing.
  /// This is a clangd extension.
  bool forceRebuild = false;
};
bool fromJSON(const llvm::json::Value &, DidChangeTextDocumentParams &,
              llvm::json::Path);

enum class FileChangeType {
  /// The file got created.
  Created = 1,
  /// The file got changed.
  Changed = 2,
  /// The file got deleted.
  Deleted = 3
};
bool fromJSON(const llvm::json::Value &E, FileChangeType &Out,
              llvm::json::Path);

struct FileEvent {
  /// The file's URI.
  URIForFile uri;
  /// The change type.
  FileChangeType type = FileChangeType::Created;
};
bool fromJSON(const llvm::json::Value &, FileEvent &, llvm::json::Path);

struct DidChangeWatchedFilesParams {
  /// The actual file events.
  std::vector<FileEvent> changes;
};
bool fromJSON(const llvm::json::Value &, DidChangeWatchedFilesParams &,
              llvm::json::Path);

struct DidChangeConfigurationParams {
  ConfigurationSettings settings;
};
bool fromJSON(const llvm::json::Value &, DidChangeConfigurationParams &,
              llvm::json::Path);

// Note: we do not parse FormattingOptions for *FormattingParams.
// In general, we use a clang-format style detected from common mechanisms
// (.clang-format files and the -fallback-style flag).
// It would be possible to override these with FormatOptions, but:
//  - the protocol makes FormatOptions mandatory, so many clients set them to
//    useless values, and we can't tell when to respect them
// - we also format in other places, where FormatOptions aren't available.

struct DocumentRangeFormattingParams {
  /// The document to format.
  TextDocumentIdentifier textDocument;

  /// The range to format
  Range range;
};
bool fromJSON(const llvm::json::Value &, DocumentRangeFormattingParams &,
              llvm::json::Path);

struct DocumentOnTypeFormattingParams {
  /// The document to format.
  TextDocumentIdentifier textDocument;

  /// The position at which this request was sent.
  Position position;

  /// The character that has been typed.
  std::string ch;
};
bool fromJSON(const llvm::json::Value &, DocumentOnTypeFormattingParams &,
              llvm::json::Path);

struct DocumentFormattingParams {
  /// The document to format.
  TextDocumentIdentifier textDocument;
};
bool fromJSON(const llvm::json::Value &, DocumentFormattingParams &,
              llvm::json::Path);

struct DocumentSymbolParams {
  // The text document to find symbols in.
  TextDocumentIdentifier textDocument;
};
bool fromJSON(const llvm::json::Value &, DocumentSymbolParams &,
              llvm::json::Path);

/// Represents a related message and source code location for a diagnostic.
/// This should be used to point to code locations that cause or related to a
/// diagnostics, e.g when duplicating a symbol in a scope.
struct DiagnosticRelatedInformation {
  /// The location of this related diagnostic information.
  Location location;
  /// The message of this related diagnostic information.
  std::string message;
};
llvm::json::Value toJSON(const DiagnosticRelatedInformation &);

enum DiagnosticTag {
  /// Unused or unnecessary code.
  ///
  /// Clients are allowed to render diagnostics with this tag faded out instead
  /// of having an error squiggle.
  Unnecessary = 1,
  /// Deprecated or obsolete code.
  ///
  /// Clients are allowed to rendered diagnostics with this tag strike through.
  Deprecated = 2,
};
llvm::json::Value toJSON(DiagnosticTag Tag);

/// Structure to capture a description for an error code.
struct CodeDescription {
  /// An URI to open with more information about the diagnostic error.
  std::string href;
};
llvm::json::Value toJSON(const CodeDescription &);

struct CodeAction;
struct Diagnostic {
  /// The range at which the message applies.
  Range range;

  /// The diagnostic's severity. Can be omitted. If omitted it is up to the
  /// client to interpret diagnostics as error, warning, info or hint.
  int severity = 0;

  /// The diagnostic's code. Can be omitted.
  std::string code;

  /// An optional property to describe the error code.
  std::optional<CodeDescription> codeDescription;

  /// A human-readable string describing the source of this
  /// diagnostic, e.g. 'typescript' or 'super lint'.
  std::string source;

  /// The diagnostic's message.
  std::string message;

  /// Additional metadata about the diagnostic.
  llvm::SmallVector<DiagnosticTag, 1> tags;

  /// An array of related diagnostic information, e.g. when symbol-names within
  /// a scope collide all definitions can be marked via this property.
  std::optional<std::vector<DiagnosticRelatedInformation>> relatedInformation;

  /// The diagnostic's category. Can be omitted.
  /// An LSP extension that's used to send the name of the category over to the
  /// client. The category typically describes the compilation stage during
  /// which the issue was produced, e.g. "Semantic Issue" or "Parse Issue".
  std::optional<std::string> category;

  /// Clangd extension: code actions related to this diagnostic.
  /// Only with capability textDocument.publishDiagnostics.codeActionsInline.
  /// (These actions can also be obtained using textDocument/codeAction).
  std::optional<std::vector<CodeAction>> codeActions;

  /// A data entry field that is preserved between a
  /// `textDocument/publishDiagnostics` notification
  /// and `textDocument/codeAction` request.
  /// Mutating users should associate their data with a unique key they can use
  /// to retrieve later on.
  llvm::json::Object data;
};
llvm::json::Value toJSON(const Diagnostic &);

bool fromJSON(const llvm::json::Value &, Diagnostic &, llvm::json::Path);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, const Diagnostic &);

struct PublishDiagnosticsParams {
  /// The URI for which diagnostic information is reported.
  URIForFile uri;
  /// An array of diagnostic information items.
  std::vector<Diagnostic> diagnostics;
  /// The version number of the document the diagnostics are published for.
  std::optional<int64_t> version;
};
llvm::json::Value toJSON(const PublishDiagnosticsParams &);

struct CodeActionContext {
  /// An array of diagnostics known on the client side overlapping the range
  /// provided to the `textDocument/codeAction` request. They are provided so
  /// that the server knows which errors are currently presented to the user for
  /// the given range. There is no guarantee that these accurately reflect the
  /// error state of the resource. The primary parameter to compute code actions
  /// is the provided range.
  std::vector<Diagnostic> diagnostics;

  /// Requested kind of actions to return.
  ///
  /// Actions not of this kind are filtered out by the client before being
  /// shown. So servers can omit computing them.
  std::vector<std::string> only;
};
bool fromJSON(const llvm::json::Value &, CodeActionContext &, llvm::json::Path);

struct CodeActionParams {
  /// The document in which the command was invoked.
  TextDocumentIdentifier textDocument;

  /// The range for which the command was invoked.
  Range range;

  /// Context carrying additional information.
  CodeActionContext context;
};
bool fromJSON(const llvm::json::Value &, CodeActionParams &, llvm::json::Path);

/// The edit should either provide changes or documentChanges. If the client
/// can handle versioned document edits and if documentChanges are present,
/// the latter are preferred over changes.
struct WorkspaceEdit {
  /// Holds changes to existing resources.
  std::optional<std::map<std::string, std::vector<TextEdit>>> changes;
  /// Versioned document edits.
  ///
  /// If a client neither supports `documentChanges` nor
  /// `workspace.workspaceEdit.resourceOperations` then only plain `TextEdit`s
  /// using the `changes` property are supported.
  std::optional<std::vector<TextDocumentEdit>> documentChanges;

  /// A map of change annotations that can be referenced in
  /// AnnotatedTextEdit.
  std::map<std::string, ChangeAnnotation> changeAnnotations;
};
bool fromJSON(const llvm::json::Value &, WorkspaceEdit &, llvm::json::Path);
llvm::json::Value toJSON(const WorkspaceEdit &WE);

/// Arguments for the 'applyTweak' command. The server sends these commands as a
/// response to the textDocument/codeAction request. The client can later send a
/// command back to the server if the user requests to execute a particular code
/// tweak.
struct TweakArgs {
  /// A file provided by the client on a textDocument/codeAction request.
  URIForFile file;
  /// A selection provided by the client on a textDocument/codeAction request.
  Range selection;
  /// ID of the tweak that should be executed. Corresponds to Tweak::id().
  std::string tweakID;
};
bool fromJSON(const llvm::json::Value &, TweakArgs &, llvm::json::Path);
llvm::json::Value toJSON(const TweakArgs &A);

struct ExecuteCommandParams {
  /// The identifier of the actual command handler.
  std::string command;

  // This is `arguments?: []any` in LSP.
  // All clangd's commands accept a single argument (or none => null).
  llvm::json::Value argument = nullptr;
};
bool fromJSON(const llvm::json::Value &, ExecuteCommandParams &,
              llvm::json::Path);

struct Command : public ExecuteCommandParams {
  std::string title;
};
llvm::json::Value toJSON(const Command &C);

/// A code action represents a change that can be performed in code, e.g. to fix
/// a problem or to refactor code.
///
/// A CodeAction must set either `edit` and/or a `command`. If both are
/// supplied, the `edit` is applied first, then the `command` is executed.
struct CodeAction {
  /// A short, human-readable, title for this code action.
  std::string title;

  /// The kind of the code action.
  /// Used to filter code actions.
  std::optional<std::string> kind;
  const static llvm::StringLiteral QUICKFIX_KIND;
  const static llvm::StringLiteral REFACTOR_KIND;
  const static llvm::StringLiteral INFO_KIND;

  /// The diagnostics that this code action resolves.
  std::optional<std::vector<Diagnostic>> diagnostics;

  /// Marks this as a preferred action. Preferred actions are used by the
  /// `auto fix` command and can be targeted by keybindings.
  /// A quick fix should be marked preferred if it properly addresses the
  /// underlying error. A refactoring should be marked preferred if it is the
  /// most reasonable choice of actions to take.
  bool isPreferred = false;

  /// The workspace edit this code action performs.
  std::optional<WorkspaceEdit> edit;

  /// A command this code action executes. If a code action provides an edit
  /// and a command, first the edit is executed and then the command.
  std::optional<Command> command;
};
llvm::json::Value toJSON(const CodeAction &);

/// Represents programming constructs like variables, classes, interfaces etc.
/// that appear in a document. Document symbols can be hierarchical and they
/// have two ranges: one that encloses its definition and one that points to its
/// most interesting range, e.g. the range of an identifier.
struct DocumentSymbol {
  /// The name of this symbol.
  std::string name;

  /// More detail for this symbol, e.g the signature of a function.
  std::string detail;

  /// The kind of this symbol.
  SymbolKind kind;

  /// Indicates if this symbol is deprecated.
  bool deprecated = false;

  /// The range enclosing this symbol not including leading/trailing whitespace
  /// but everything else like comments. This information is typically used to
  /// determine if the clients cursor is inside the symbol to reveal in the
  /// symbol in the UI.
  Range range;

  /// The range that should be selected and revealed when this symbol is being
  /// picked, e.g the name of a function. Must be contained by the `range`.
  Range selectionRange;

  /// Children of this symbol, e.g. properties of a class.
  std::vector<DocumentSymbol> children;
};
llvm::raw_ostream &operator<<(llvm::raw_ostream &O, const DocumentSymbol &S);
llvm::json::Value toJSON(const DocumentSymbol &S);

/// Represents information about programming constructs like variables, classes,
/// interfaces etc.
struct SymbolInformation {
  /// The name of this symbol.
  std::string name;

  /// The kind of this symbol.
  SymbolKind kind;

  /// The location of this symbol.
  Location location;

  /// The name of the symbol containing this symbol.
  std::string containerName;

  /// The score that clangd calculates to rank the returned symbols.
  /// This excludes the fuzzy-matching score between `name` and the query.
  /// (Specifically, the last ::-separated component).
  /// This can be used to re-rank results as the user types, using client-side
  /// fuzzy-matching (that score should be multiplied with this one).
  /// This is a clangd extension, set only for workspace/symbol responses.
  std::optional<float> score;
};
llvm::json::Value toJSON(const SymbolInformation &);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, const SymbolInformation &);

/// Represents information about identifier.
/// This is returned from textDocument/symbolInfo, which is a clangd extension.
struct SymbolDetails {
  std::string name;

  std::string containerName;

  /// Unified Symbol Resolution identifier
  /// This is an opaque string uniquely identifying a symbol.
  /// Unlike SymbolID, it is variable-length and somewhat human-readable.
  /// It is a common representation across several clang tools.
  /// (See USRGeneration.h)
  std::string USR;

  SymbolID ID;

  std::optional<Location> declarationRange;

  std::optional<Location> definitionRange;
};
llvm::json::Value toJSON(const SymbolDetails &);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, const SymbolDetails &);
bool operator==(const SymbolDetails &, const SymbolDetails &);

/// The parameters of a Workspace Symbol Request.
struct WorkspaceSymbolParams {
  /// A query string to filter symbols by.
  /// Clients may send an empty string here to request all the symbols.
  std::string query;

  /// Max results to return, overriding global default. 0 means no limit.
  /// Clangd extension.
  std::optional<int> limit;
};
bool fromJSON(const llvm::json::Value &, WorkspaceSymbolParams &,
              llvm::json::Path);

struct ApplyWorkspaceEditParams {
  WorkspaceEdit edit;
};
llvm::json::Value toJSON(const ApplyWorkspaceEditParams &);

struct ApplyWorkspaceEditResponse {
  bool applied = true;
  std::optional<std::string> failureReason;
};
bool fromJSON(const llvm::json::Value &, ApplyWorkspaceEditResponse &,
              llvm::json::Path);

struct TextDocumentPositionParams {
  /// The text document.
  TextDocumentIdentifier textDocument;

  /// The position inside the text document.
  Position position;
};
bool fromJSON(const llvm::json::Value &, TextDocumentPositionParams &,
              llvm::json::Path);

enum class CompletionTriggerKind {
  /// Completion was triggered by typing an identifier (24x7 code
  /// complete), manual invocation (e.g Ctrl+Space) or via API.
  Invoked = 1,
  /// Completion was triggered by a trigger character specified by
  /// the `triggerCharacters` properties of the `CompletionRegistrationOptions`.
  TriggerCharacter = 2,
  /// Completion was re-triggered as the current completion list is incomplete.
  TriggerTriggerForIncompleteCompletions = 3
};

struct CompletionContext {
  /// How the completion was triggered.
  CompletionTriggerKind triggerKind = CompletionTriggerKind::Invoked;
  /// The trigger character (a single character) that has trigger code complete.
  /// Is undefined if `triggerKind !== CompletionTriggerKind.TriggerCharacter`
  std::string triggerCharacter;
};
bool fromJSON(const llvm::json::Value &, CompletionContext &, llvm::json::Path);

struct CompletionParams : TextDocumentPositionParams {
  CompletionContext context;

  /// Max results to return, overriding global default. 0 means no limit.
  /// Clangd extension.
  std::optional<int> limit;
};
bool fromJSON(const llvm::json::Value &, CompletionParams &, llvm::json::Path);

struct MarkupContent {
  MarkupKind kind = MarkupKind::PlainText;
  std::string value;
};
llvm::json::Value toJSON(const MarkupContent &MC);

struct Hover {
  /// The hover's content
  MarkupContent contents;

  /// An optional range is a range inside a text document
  /// that is used to visualize a hover, e.g. by changing the background color.
  std::optional<Range> range;
};
llvm::json::Value toJSON(const Hover &H);

/// Defines whether the insert text in a completion item should be interpreted
/// as plain text or a snippet.
enum class InsertTextFormat {
  Missing = 0,
  /// The primary text to be inserted is treated as a plain string.
  PlainText = 1,
  /// The primary text to be inserted is treated as a snippet.
  ///
  /// A snippet can define tab stops and placeholders with `$1`, `$2`
  /// and `${3:foo}`. `$0` defines the final tab stop, it defaults to the end
  /// of the snippet. Placeholders with equal identifiers are linked, that is
  /// typing in one will update others too.
  ///
  /// See also:
  /// https://github.com/Microsoft/vscode/blob/main/src/vs/editor/contrib/snippet/snippet.md
  Snippet = 2,
};

/// Additional details for a completion item label.
struct CompletionItemLabelDetails {
  /// An optional string which is rendered less prominently directly after label
  /// without any spacing. Should be used for function signatures or type
  /// annotations.
  std::string detail;

  /// An optional string which is rendered less prominently after
  /// CompletionItemLabelDetails.detail. Should be used for fully qualified
  /// names or file path.
  std::string description;
};
llvm::json::Value toJSON(const CompletionItemLabelDetails &);

struct CompletionItem {
  /// The label of this completion item. By default also the text that is
  /// inserted when selecting this completion.
  std::string label;

  /// Additional details for the label.
  std::optional<CompletionItemLabelDetails> labelDetails;

  /// The kind of this completion item. Based of the kind an icon is chosen by
  /// the editor.
  CompletionItemKind kind = CompletionItemKind::Missing;

  /// A human-readable string with additional information about this item, like
  /// type or symbol information.
  std::string detail;

  /// A human-readable string that represents a doc-comment.
  std::optional<MarkupContent> documentation;

  /// A string that should be used when comparing this item with other items.
  /// When `falsy` the label is used.
  std::string sortText;

  /// A string that should be used when filtering a set of completion items.
  /// When `falsy` the label is used.
  std::string filterText;

  /// A string that should be inserted to a document when selecting this
  /// completion. When `falsy` the label is used.
  std::string insertText;

  /// The format of the insert text. The format applies to both the `insertText`
  /// property and the `newText` property of a provided `textEdit`.
  InsertTextFormat insertTextFormat = InsertTextFormat::Missing;

  /// An edit which is applied to a document when selecting this completion.
  /// When an edit is provided `insertText` is ignored.
  ///
  /// Note: The range of the edit must be a single line range and it must
  /// contain the position at which completion has been requested.
  std::optional<TextEdit> textEdit;

  /// An optional array of additional text edits that are applied when selecting
  /// this completion. Edits must not overlap with the main edit nor with
  /// themselves.
  std::vector<TextEdit> additionalTextEdits;

  /// Indicates if this item is deprecated.
  bool deprecated = false;

  /// The score that clangd calculates to rank the returned completions.
  /// This excludes the fuzzy-match between `filterText` and the partial word.
  /// This can be used to re-rank results as the user types, using client-side
  /// fuzzy-matching (that score should be multiplied with this one).
  /// This is a clangd extension.
  float score = 0.f;

  // TODO: Add custom commitCharacters for some of the completion items. For
  // example, it makes sense to use () only for the functions.
  // TODO(krasimir): The following optional fields defined by the language
  // server protocol are unsupported:
  //
  // data?: any - A data entry field that is preserved on a completion item
  //              between a completion and a completion resolve request.
};
llvm::json::Value toJSON(const CompletionItem &);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, const CompletionItem &);

/// Remove the labelDetails field (for clients that don't support it).
/// Places the information into other fields of the completion item.
void removeCompletionLabelDetails(CompletionItem &);

bool operator<(const CompletionItem &, const CompletionItem &);

/// Represents a collection of completion items to be presented in the editor.
struct CompletionList {
  /// The list is not complete. Further typing should result in recomputing the
  /// list.
  bool isIncomplete = false;

  /// The completion items.
  std::vector<CompletionItem> items;
};
llvm::json::Value toJSON(const CompletionList &);

/// A single parameter of a particular signature.
struct ParameterInformation {

  /// The label of this parameter. Ignored when labelOffsets is set.
  std::string labelString;

  /// Inclusive start and exclusive end offsets withing the containing signature
  /// label.
  /// Offsets are computed by lspLength(), which counts UTF-16 code units by
  /// default but that can be overriden, see its documentation for details.
  std::optional<std::pair<unsigned, unsigned>> labelOffsets;

  /// The documentation of this parameter. Optional.
  std::string documentation;
};
llvm::json::Value toJSON(const ParameterInformation &);

/// Represents the signature of something callable.
struct SignatureInformation {

  /// The label of this signature. Mandatory.
  std::string label;

  /// The documentation of this signature. Optional.
  MarkupContent documentation;

  /// The parameters of this signature.
  std::vector<ParameterInformation> parameters;
};
llvm::json::Value toJSON(const SignatureInformation &);
llvm::raw_ostream &operator<<(llvm::raw_ostream &,
                              const SignatureInformation &);

/// Represents the signature of a callable.
struct SignatureHelp {

  /// The resulting signatures.
  std::vector<SignatureInformation> signatures;

  /// The active signature.
  int activeSignature = 0;

  /// The active parameter of the active signature.
  int activeParameter = 0;

  /// Position of the start of the argument list, including opening paren. e.g.
  /// foo("first arg",   "second arg",
  ///    ^-argListStart   ^-cursor
  /// This is a clangd-specific extension, it is only available via C++ API and
  /// not currently serialized for the LSP.
  Position argListStart;
};
llvm::json::Value toJSON(const SignatureHelp &);

struct RenameParams {
  /// The document that was opened.
  TextDocumentIdentifier textDocument;

  /// The position at which this request was sent.
  Position position;

  /// The new name of the symbol.
  std::string newName;
};
bool fromJSON(const llvm::json::Value &, RenameParams &, llvm::json::Path);
llvm::json::Value toJSON(const RenameParams &);

struct PrepareRenameResult {
  /// Range of the string to rename.
  Range range;
  /// Placeholder text to use in the editor if non-empty.
  std::string placeholder;
};
llvm::json::Value toJSON(const PrepareRenameResult &PRR);

enum class DocumentHighlightKind { Text = 1, Read = 2, Write = 3 };

/// A document highlight is a range inside a text document which deserves
/// special attention. Usually a document highlight is visualized by changing
/// the background color of its range.

struct DocumentHighlight {
  /// The range this highlight applies to.
  Range range;

  /// The highlight kind, default is DocumentHighlightKind.Text.
  DocumentHighlightKind kind = DocumentHighlightKind::Text;

  friend bool operator<(const DocumentHighlight &LHS,
                        const DocumentHighlight &RHS) {
    int LHSKind = static_cast<int>(LHS.kind);
    int RHSKind = static_cast<int>(RHS.kind);
    return std::tie(LHS.range, LHSKind) < std::tie(RHS.range, RHSKind);
  }

  friend bool operator==(const DocumentHighlight &LHS,
                         const DocumentHighlight &RHS) {
    return LHS.kind == RHS.kind && LHS.range == RHS.range;
  }
};
llvm::json::Value toJSON(const DocumentHighlight &DH);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, const DocumentHighlight &);

enum class TypeHierarchyDirection { Children = 0, Parents = 1, Both = 2 };
bool fromJSON(const llvm::json::Value &E, TypeHierarchyDirection &Out,
              llvm::json::Path);

/// The type hierarchy params is an extension of the
/// `TextDocumentPositionsParams` with optional properties which can be used to
/// eagerly resolve the item when requesting from the server.
struct TypeHierarchyPrepareParams : public TextDocumentPositionParams {
  /// The hierarchy levels to resolve. `0` indicates no level.
  /// This is a clangd extension.
  int resolve = 0;

  /// The direction of the hierarchy levels to resolve.
  /// This is a clangd extension.
  TypeHierarchyDirection direction = TypeHierarchyDirection::Parents;
};
bool fromJSON(const llvm::json::Value &, TypeHierarchyPrepareParams &,
              llvm::json::Path);

struct TypeHierarchyItem {
  /// The name of this item.
  std::string name;

  /// The kind of this item.
  SymbolKind kind;

  /// More detail for this item, e.g. the signature of a function.
  std::optional<std::string> detail;

  /// The resource identifier of this item.
  URIForFile uri;

  /// The range enclosing this symbol not including leading/trailing whitespace
  /// but everything else, e.g. comments and code.
  Range range;

  /// The range that should be selected and revealed when this symbol is being
  /// picked, e.g. the name of a function. Must be contained by the `range`.
  Range selectionRange;

  /// Used to resolve a client provided item back.
  struct ResolveParams {
    SymbolID symbolID;
    /// std::nullopt means parents aren't resolved and empty is no parents.
    std::optional<std::vector<ResolveParams>> parents;
  };
  /// A data entry field that is preserved between a type hierarchy prepare and
  /// supertypes or subtypes requests. It could also be used to identify the
  /// type hierarchy in the server, helping improve the performance on resolving
  /// supertypes and subtypes.
  ResolveParams data;

  /// `true` if the hierarchy item is deprecated. Otherwise, `false`.
  /// This is a clangd exntesion.
  bool deprecated = false;

  /// This is a clangd exntesion.
  std::optional<std::vector<TypeHierarchyItem>> parents;

  /// If this type hierarchy item is resolved, it contains the direct children
  /// of the current item. Could be empty if the item does not have any
  /// descendants. If not defined, the children have not been resolved.
  /// This is a clangd exntesion.
  std::optional<std::vector<TypeHierarchyItem>> children;
};
llvm::json::Value toJSON(const TypeHierarchyItem::ResolveParams &);
bool fromJSON(const TypeHierarchyItem::ResolveParams &);
llvm::json::Value toJSON(const TypeHierarchyItem &);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, const TypeHierarchyItem &);
bool fromJSON(const llvm::json::Value &, TypeHierarchyItem &, llvm::json::Path);

/// Parameters for the `typeHierarchy/resolve` request.
struct ResolveTypeHierarchyItemParams {
  /// The item to resolve.
  TypeHierarchyItem item;

  /// The hierarchy levels to resolve. `0` indicates no level.
  int resolve;

  /// The direction of the hierarchy levels to resolve.
  TypeHierarchyDirection direction;
};
bool fromJSON(const llvm::json::Value &, ResolveTypeHierarchyItemParams &,
              llvm::json::Path);

enum class SymbolTag { Deprecated = 1 };
llvm::json::Value toJSON(SymbolTag);

/// The parameter of a `textDocument/prepareCallHierarchy` request.
struct CallHierarchyPrepareParams : public TextDocumentPositionParams {};

/// Represents programming constructs like functions or constructors
/// in the context of call hierarchy.
struct CallHierarchyItem {
  /// The name of this item.
  std::string name;

  /// The kind of this item.
  SymbolKind kind;

  /// Tags for this item.
  std::vector<SymbolTag> tags;

  /// More detaill for this item, e.g. the signature of a function.
  std::string detail;

  /// The resource identifier of this item.
  URIForFile uri;

  /// The range enclosing this symbol not including leading / trailing
  /// whitespace but everything else, e.g. comments and code.
  Range range;

  /// The range that should be selected and revealed when this symbol
  /// is being picked, e.g. the name of a function.
  /// Must be contained by `Rng`.
  Range selectionRange;

  /// An optional 'data' field, which can be used to identify a call
  /// hierarchy item in an incomingCalls or outgoingCalls request.
  std::string data;
};
llvm::json::Value toJSON(const CallHierarchyItem &);
bool fromJSON(const llvm::json::Value &, CallHierarchyItem &, llvm::json::Path);

/// The parameter of a `callHierarchy/incomingCalls` request.
struct CallHierarchyIncomingCallsParams {
  CallHierarchyItem item;
};
bool fromJSON(const llvm::json::Value &, CallHierarchyIncomingCallsParams &,
              llvm::json::Path);

/// Represents an incoming call, e.g. a caller of a method or constructor.
struct CallHierarchyIncomingCall {
  /// The item that makes the call.
  CallHierarchyItem from;

  /// The range at which the calls appear.
  /// This is relative to the caller denoted by `From`.
  std::vector<Range> fromRanges;
};
llvm::json::Value toJSON(const CallHierarchyIncomingCall &);

/// The parameter of a `callHierarchy/outgoingCalls` request.
struct CallHierarchyOutgoingCallsParams {
  CallHierarchyItem item;
};
bool fromJSON(const llvm::json::Value &, CallHierarchyOutgoingCallsParams &,
              llvm::json::Path);

/// Represents an outgoing call, e.g. calling a getter from a method or
/// a method from a constructor etc.
struct CallHierarchyOutgoingCall {
  /// The item that is called.
  CallHierarchyItem to;

  /// The range at which this item is called.
  /// This is the range relative to the caller, and not `To`.
  std::vector<Range> fromRanges;
};
llvm::json::Value toJSON(const CallHierarchyOutgoingCall &);

/// A parameter literal used in inlay hint requests.
struct InlayHintsParams {
  /// The text document.
  TextDocumentIdentifier textDocument;

  /// The visible document range for which inlay hints should be computed.
  ///
  /// std::nullopt is a clangd extension, which hints for computing hints on the
  /// whole file.
  std::optional<Range> range;
};
bool fromJSON(const llvm::json::Value &, InlayHintsParams &, llvm::json::Path);

/// Inlay hint kinds.
enum class InlayHintKind {
  /// An inlay hint that for a type annotation.
  ///
  /// An example of a type hint is a hint in this position:
  ///    auto var ^ = expr;
  /// which shows the deduced type of the variable.
  Type = 1,

  /// An inlay hint that is for a parameter.
  ///
  /// An example of a parameter hint is a hint in this position:
  ///    func(^arg);
  /// which shows the name of the corresponding parameter.
  Parameter = 2,

  /// A hint before an element of an aggregate braced initializer list,
  /// indicating what it is initializing.
  ///   Pair{^1, ^2};
  /// Uses designator syntax, e.g. `.first:`.
  /// This is a clangd extension.
  Designator = 3,

  /// A hint after function, type or namespace definition, indicating the
  /// defined symbol name of the definition.
  ///
  /// An example of a decl name hint in this position:
  ///    void func() {
  ///    } ^
  /// Uses comment-like syntax like "// func".
  /// This is a clangd extension.
  BlockEnd = 4,

  /// An inlay hint that is for a variable captured implicitly in a lambda.
  ///
  /// An example of parameter hint for implicit lambda captures:
  ///    [&^] { return A; };
  /// Adds an inlay hint ": A".
  LambdaCapture = 5,

  /// An inlay hint that is for a default argument.
  ///
  /// An example of a parameter hint for a default argument:
  ///    void foo(bool A = true);
  ///    foo(^);
  /// Adds an inlay hint "A = true".
  DefaultArgument = 6,

  /// A hint for an implicit default initializer.
  ///
  /// An example of implicit default construction:
  ///    MyObject O^;
  /// Adds a hint for "{}".
  DefaultInit = 7,

  /// A hint for an implicit usage of this pointer.
  ///
  /// An example of implicit this pointer:
  ///    struct MyObject { int foo; int bar(); };
  ///    MyObject::foo() { return ^bar; }
  /// Adds a hinted "this->".
  ImplicitThis = 8,

  /// Other ideas for hints that are not currently implemented:
  ///
  /// * Chaining hints, showing the types of intermediate expressions
  ///   in a chain of function calls.
  /// * Hints indicating implicit conversions or implicit constructor calls.
};
llvm::json::Value toJSON(const InlayHintKind &);

/// An inlay hint label part allows for interactive and composite labels
/// of inlay hints.
struct InlayHintLabelPart {

  InlayHintLabelPart() = default;

  InlayHintLabelPart(std::string value,
                     std::optional<Location> location = std::nullopt)
      : value(std::move(value)), location(std::move(location)) {}

  /// The value of this label part.
  std::string value;

  /// The tooltip text when you hover over this label part. Depending on
  /// the client capability `inlayHint.resolveSupport`, clients might resolve
  /// this property late using the resolve request.
  std::optional<MarkupContent> tooltip;

  /// An optional source code location that represents this
  /// label part.
  ///
  /// The editor will use this location for the hover and for code navigation
  /// features: This part will become a clickable link that resolves to the
  /// definition of the symbol at the given location (not necessarily the
  /// location itself), it shows the hover that shows at the given location,
  /// and it shows a context menu with further code navigation commands.
  ///
  /// Depending on the client capability `inlayHint.resolveSupport` clients
  /// might resolve this property late using the resolve request.
  std::optional<Location> location;

  /// An optional command for this label part.
  ///
  /// Depending on the client capability `inlayHint.resolveSupport` clients
  /// might resolve this property late using the resolve request.
  std::optional<Command> command;
};
llvm::json::Value toJSON(const InlayHintLabelPart &);
bool operator==(const InlayHintLabelPart &, const InlayHintLabelPart &);
bool operator<(const InlayHintLabelPart &, const InlayHintLabelPart &);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, const InlayHintLabelPart &);

/// Inlay hint information.
struct InlayHint {
  /// The position of this hint.
  Position position;

  /// The label of this hint. A human readable string or an array of
  /// InlayHintLabelPart label parts.
  ///
  /// *Note* that neither the string nor the label part can be empty.
  std::vector<InlayHintLabelPart> label;

  /// The kind of this hint. Can be omitted in which case the client should fall
  /// back to a reasonable default.
  InlayHintKind kind;

  /// Render padding before the hint.
  ///
  /// Note: Padding should use the editor's background color, not the
  /// background color of the hint itself. That means padding can be used
  /// to visually align/separate an inlay hint.
  bool paddingLeft = false;

  /// Render padding after the hint.
  ///
  /// Note: Padding should use the editor's background color, not the
  /// background color of the hint itself. That means padding can be used
  /// to visually align/separate an inlay hint.
  bool paddingRight = false;

  /// The range of source code to which the hint applies.
  ///
  /// For example, a parameter hint may have the argument as its range.
  /// The range allows clients more flexibility of when/how to display the hint.
  /// This is an (unserialized) clangd extension.
  Range range;

  /// Join the label[].value together.
  std::string joinLabels() const;
};
llvm::json::Value toJSON(const InlayHint &);
bool operator==(const InlayHint &, const InlayHint &);
bool operator<(const InlayHint &, const InlayHint &);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, InlayHintKind);

struct ReferenceContext {
  /// Include the declaration of the current symbol.
  bool includeDeclaration = false;
};

struct ReferenceParams : public TextDocumentPositionParams {
  ReferenceContext context;
};
bool fromJSON(const llvm::json::Value &, ReferenceParams &, llvm::json::Path);

/// Clangd extension: indicates the current state of the file in clangd,
/// sent from server via the `textDocument/clangd.fileStatus` notification.
struct FileStatus {
  /// The text document's URI.
  URIForFile uri;
  /// The human-readable string presents the current state of the file, can be
  /// shown in the UI (e.g. status bar).
  std::string state;
  // FIXME: add detail messages.
};
llvm::json::Value toJSON(const FileStatus &);

/// Specifies a single semantic token in the document.
/// This struct is not part of LSP, which just encodes lists of tokens as
/// arrays of numbers directly.
struct SemanticToken {
  /// token line number, relative to the previous token
  unsigned deltaLine = 0;
  /// token start character, relative to the previous token
  /// (relative to 0 or the previous token's start if they are on the same line)
  unsigned deltaStart = 0;
  /// the length of the token. A token cannot be multiline
  unsigned length = 0;
  /// will be looked up in `SemanticTokensLegend.tokenTypes`
  unsigned tokenType = 0;
  /// each set bit will be looked up in `SemanticTokensLegend.tokenModifiers`
  unsigned tokenModifiers = 0;
};
bool operator==(const SemanticToken &, const SemanticToken &);

/// A versioned set of tokens.
struct SemanticTokens {
  // An optional result id. If provided and clients support delta updating
  // the client will include the result id in the next semantic token request.
  // A server can then instead of computing all semantic tokens again simply
  // send a delta.
  std::string resultId;

  /// The actual tokens.
  std::vector<SemanticToken> tokens; // encoded as a flat integer array.
};
llvm::json::Value toJSON(const SemanticTokens &);

/// Body of textDocument/semanticTokens/full request.
struct SemanticTokensParams {
  /// The text document.
  TextDocumentIdentifier textDocument;
};
bool fromJSON(const llvm::json::Value &, SemanticTokensParams &,
              llvm::json::Path);

/// Body of textDocument/semanticTokens/full/delta request.
/// Requests the changes in semantic tokens since a previous response.
struct SemanticTokensDeltaParams {
  /// The text document.
  TextDocumentIdentifier textDocument;
  /// The previous result id.
  std::string previousResultId;
};
bool fromJSON(const llvm::json::Value &Params, SemanticTokensDeltaParams &R,
              llvm::json::Path);

/// Describes a replacement of a contiguous range of semanticTokens.
struct SemanticTokensEdit {
  // LSP specifies `start` and `deleteCount` which are relative to the array
  // encoding of the previous tokens.
  // We use token counts instead, and translate when serializing this struct.
  unsigned startToken = 0;
  unsigned deleteTokens = 0;
  std::vector<SemanticToken> tokens; // encoded as a flat integer array
};
llvm::json::Value toJSON(const SemanticTokensEdit &);

/// This models LSP SemanticTokensDelta | SemanticTokens, which is the result of
/// textDocument/semanticTokens/full/delta.
struct SemanticTokensOrDelta {
  std::string resultId;
  /// Set if we computed edits relative to a previous set of tokens.
  std::optional<std::vector<SemanticTokensEdit>> edits;
  /// Set if we computed a fresh set of tokens.
  std::optional<std::vector<SemanticToken>> tokens; // encoded as integer array
};
llvm::json::Value toJSON(const SemanticTokensOrDelta &);

/// Parameters for the inactive regions (server-side) push notification.
/// This is a clangd extension.
struct InactiveRegionsParams {
  /// The textdocument these inactive regions belong to.
  TextDocumentIdentifier TextDocument;
  /// The inactive regions that should be sent.
  std::vector<Range> InactiveRegions;
};
llvm::json::Value toJSON(const InactiveRegionsParams &InactiveRegions);

struct SelectionRangeParams {
  /// The text document.
  TextDocumentIdentifier textDocument;

  /// The positions inside the text document.
  std::vector<Position> positions;
};
bool fromJSON(const llvm::json::Value &, SelectionRangeParams &,
              llvm::json::Path);

struct SelectionRange {
  /**
   * The range of this selection range.
   */
  Range range;
  /**
   * The parent selection range containing this range. Therefore `parent.range`
   * must contain `this.range`.
   */
  std::unique_ptr<SelectionRange> parent;
};
llvm::json::Value toJSON(const SelectionRange &);

/// Parameters for the document link request.
struct DocumentLinkParams {
  /// The document to provide document links for.
  TextDocumentIdentifier textDocument;
};
bool fromJSON(const llvm::json::Value &, DocumentLinkParams &,
              llvm::json::Path);

/// A range in a text document that links to an internal or external resource,
/// like another text document or a web site.
struct DocumentLink {
  /// The range this link applies to.
  Range range;

  /// The uri this link points to. If missing a resolve request is sent later.
  URIForFile target;

  // TODO(forster): The following optional fields defined by the language
  // server protocol are unsupported:
  //
  // data?: any - A data entry field that is preserved on a document link
  //              between a DocumentLinkRequest and a
  //              DocumentLinkResolveRequest.

  friend bool operator==(const DocumentLink &LHS, const DocumentLink &RHS) {
    return LHS.range == RHS.range && LHS.target == RHS.target;
  }

  friend bool operator!=(const DocumentLink &LHS, const DocumentLink &RHS) {
    return !(LHS == RHS);
  }
};
llvm::json::Value toJSON(const DocumentLink &DocumentLink);

// FIXME(kirillbobyrev): Add FoldingRangeClientCapabilities so we can support
// per-line-folding editors.
struct FoldingRangeParams {
  TextDocumentIdentifier textDocument;
};
bool fromJSON(const llvm::json::Value &, FoldingRangeParams &,
              llvm::json::Path);

/// Stores information about a region of code that can be folded.
struct FoldingRange {
  unsigned startLine = 0;
  unsigned startCharacter;
  unsigned endLine = 0;
  unsigned endCharacter;

  const static llvm::StringLiteral REGION_KIND;
  const static llvm::StringLiteral COMMENT_KIND;
  const static llvm::StringLiteral IMPORT_KIND;
  std::string kind;
};
llvm::json::Value toJSON(const FoldingRange &Range);

/// Keys starting with an underscore(_) represent leaves, e.g. _total or _self
/// for memory usage of whole subtree or only that specific node in bytes. All
/// other keys represents children. An example:
///   {
///     "_self": 0,
///     "_total": 8,
///     "child1": {
///       "_self": 4,
///       "_total": 4,
///     }
///     "child2": {
///       "_self": 2,
///       "_total": 4,
///       "child_deep": {
///         "_self": 2,
///         "_total": 2,
///       }
///     }
///   }
llvm::json::Value toJSON(const MemoryTree &MT);

/// Payload for textDocument/ast request.
/// This request is a clangd extension.
struct ASTParams {
  /// The text document.
  TextDocumentIdentifier textDocument;

  /// The position of the node to be dumped.
  /// The highest-level node that entirely contains the range will be returned.
  /// If no range is given, the root translation unit node will be returned.
  std::optional<Range> range;
};
bool fromJSON(const llvm::json::Value &, ASTParams &, llvm::json::Path);

/// Simplified description of a clang AST node.
/// This is clangd's internal representation of C++ code.
struct ASTNode {
  /// The general kind of node, such as "expression"
  /// Corresponds to the base AST node type such as Expr.
  std::string role;
  /// The specific kind of node this is, such as "BinaryOperator".
  /// This is usually a concrete node class (with Expr etc suffix dropped).
  /// When there's no hierarchy (e.g. TemplateName), the variant (NameKind).
  std::string kind;
  /// Brief additional information, such as "||" for the particular operator.
  /// The information included depends on the node kind, and may be empty.
  std::string detail;
  /// A one-line dump of detailed information about the node.
  /// This includes role/kind/description information, but is rather cryptic.
  /// It is similar to the output from `clang -Xclang -ast-dump`.
  /// May be empty for certain types of nodes.
  std::string arcana;
  /// The range of the original source file covered by this node.
  /// May be missing for implicit nodes, or those created by macro expansion.
  std::optional<Range> range;
  /// Nodes nested within this one, such as the operands of a BinaryOperator.
  std::vector<ASTNode> children;
};
llvm::json::Value toJSON(const ASTNode &);
llvm::raw_ostream &operator<<(llvm::raw_ostream &, const ASTNode &);

} // namespace clangd
} // namespace clang

namespace llvm {

template <> struct DenseMapInfo<clang::clangd::Range> {
  using Range = clang::clangd::Range;
  static inline Range getEmptyKey() {
    static clang::clangd::Position Tomb{-1, -1};
    static Range R{Tomb, Tomb};
    return R;
  }
  static inline Range getTombstoneKey() {
    static clang::clangd::Position Tomb{-2, -2};
    static Range R{Tomb, Tomb};
    return R;
  }
  static unsigned getHashValue(const Range &Val) {
    return llvm::hash_combine(Val.start.line, Val.start.character, Val.end.line,
                              Val.end.character);
  }
  static bool isEqual(const Range &LHS, const Range &RHS) {
    return std::tie(LHS.start, LHS.end) == std::tie(RHS.start, RHS.end);
  }
};

template <> struct format_provider<clang::clangd::Position> {
  static void format(const clang::clangd::Position &Pos, raw_ostream &OS,
                     StringRef Style) {
    assert(Style.empty() && "style modifiers for this type are not supported");
    OS << Pos;
  }
};
} // namespace llvm

// NOLINTEND(readability-identifier-naming)

#endif
