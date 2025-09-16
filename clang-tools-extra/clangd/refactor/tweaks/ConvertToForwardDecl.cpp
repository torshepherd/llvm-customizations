//===--- SpecialMembers.cpp - Generate C++ special member functions -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#include "ParsedAST.h"
#include "Protocol.h"
#include "refactor/InsertionPoint.h"
#include "refactor/Tweak.h"
#include "clang/AST/DeclCXX.h"
#include "clang/Sema/Sema.h"
#include "clang/Tooling/Core/Replacement.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include <string>

namespace clang {
namespace clangd {
namespace {

/// Finds a spelled token that the cursor is pointing at.
static const syntax::Token *
findTokenUnderCursor(const SourceManager &SM,
                     llvm::ArrayRef<syntax::Token> Spelled,
                     unsigned CursorOffset) {
  // Find the token that strats after the offset, then look at a previous one.
  auto *It = llvm::partition_point(Spelled, [&](const syntax::Token &T) {
    assert(T.location().isFileID());
    return SM.getFileOffset(T.location()) <= CursorOffset;
  });
  if (It == Spelled.begin())
    return nullptr;
  // Check the token we found actually touches the cursor position.
  --It;
  return It->range(SM).touches(CursorOffset) ? It : nullptr;
}

static const syntax::Token *
findIdentifierUnderCursor(const syntax::TokenBuffer &Tokens,
                          SourceLocation Cursor) {
  assert(Cursor.isFileID());

  auto &SM = Tokens.sourceManager();
  auto Spelled = Tokens.spelledTokens(SM.getFileID(Cursor));

  auto *T = findTokenUnderCursor(SM, Spelled, SM.getFileOffset(Cursor));
  if (!T)
    return nullptr;
  if (T->kind() == tok::pp_include)
    return T;
  // Also try the previous token when the cursor is at the boundary, e.g.
  //   FOO^()
  //   FOO^+
  if (T == Spelled.begin())
    return nullptr;
  --T;
  if (T->endLocation() != Cursor || T->kind() != tok::pp_include)
    return nullptr;
  return T;
}

// A tweak that converts an include directive into a list of
// forward-declarations.
//
// e.g. given `#include "llvm/include/llvm/ADT/SmallVector.h", where the source
// code makes use of SmallVector and Array`, produces:
//   struct S {
//     S(const S&) = default;
//     S(S&&) = default;
//     S &operator=(const S&) = default;
//     S &operator=(S&&) = default;
//   };
//
// Added members are defaulted or deleted to approximately preserve semantics.
// (May not be a strict no-op when they were not implicitly declared).
//
// Having these spelled out is useful:
//  - to understand the implicit behavior
//  - to avoid relying on the implicit behavior
//  - as a baseline for explicit modification
class SpecialMembers : public Tweak {
public:
  const char *id() const final;
  llvm::StringLiteral kind() const override {
    return CodeAction::REFACTOR_KIND;
  }
  std::string title() const override {
    std::string Out = "Convert include to forward-decl of ";
    auto Front = llvm::ArrayRef(SymbolsToDeclare).take_front(SymbolNamesLimit);

    llvm::interleave(
        Front,
        [&](const NamedDecl *ND) {
          Out.append(ND->getDeclName().getAsString());
        },
        [&] { Out.append(", "); });
    if (SymbolsToDeclare.size() > Front.size()) {
      Out.append(" and ");
      Out.append(std::to_string(SymbolsToDeclare.size() - Front.size()));
      Out.append(" more");
    }

    return Out;
  }

  bool prepare(const Selection &Inputs) override{
      // Trigger only on include directives
      Inputs.Cursor}

  Expected<Effect> apply(const Selection &Inputs) override {
    // Implicit special members are created lazily by clang.
    // We need them so we can tell whether they should be =default or =delete.
    Inputs.AST->getSema().ForceDeclarationOfImplicitMembers(Class);
    std::string Code = buildSpecialMemberDeclarations(*Class);

    // Prefer to place the new members...
    std::vector<Anchor> Anchors = {
        // Below the default constructor
        {[](const Decl *D) {
           if (const auto *CCD = llvm::dyn_cast<CXXConstructorDecl>(D))
             return CCD->isDefaultConstructor();
           return false;
         },
         Anchor::Below},
        // Above existing constructors
        {[](const Decl *D) { return llvm::isa<CXXConstructorDecl>(D); },
         Anchor::Above},
        // At the top of the public section
        {[](const Decl *D) { return true; }, Anchor::Above},
    };
    auto Edit = insertDecl(Code, *Class, std::move(Anchors), AS_public);
    if (!Edit)
      return Edit.takeError();
    return Effect::mainFileEdit(Inputs.AST->getSourceManager(),
                                tooling::Replacements{std::move(*Edit)});
  }

private:
  static const size_t SymbolNamesLimit = 5; // Same as Hover info
  llvm::SmallVector<NamedDecl *> SymbolsToDeclare{};
};
REGISTER_TWEAK(SpecialMembers)

} // namespace
} // namespace clangd
} // namespace clang
