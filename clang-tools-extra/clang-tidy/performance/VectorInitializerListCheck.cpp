//===--- VectorInitializerListCheck.cpp - clang-tidy ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "VectorInitializerListCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/Stmt.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Lex/Lexer.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/FormatVariadic.h"
#include <memory>
#include <string>
#include <vector>

using namespace clang::ast_matchers;

namespace clang::tidy::performance {

struct Test {
  std::string Data;
  int Data2;
};

std::vector<Test> TestFn(int i) {
  // Things to handle

  // Variable declarations
  // Var -> ExprWithCleanups -> CXXConstruct -> CXXStdInitializerList
  std::vector<std::string> V({"", "", {}});
  // Var -> ExprWithCleanups -> CXXConstruct -> CXXStdInitializerList
  std::vector<std::string> V1{"", ""};
  // Var -> ExprWithCleanups -> CXXConstruct -> CXXStdInitializerList
  std::vector<Test> V2 = {{"hi", 1}, {}, {"there", 3}};
  // Var -> ExprWithCleanups -> CXXBindTemporary -> CXXTemporaryObject ->
  // CXXStdInitializerList
  std::vector<Test> V3 = std::vector<Test>{{"hi", 1}, {}, {"there", 3}};
  // Var -> ExprWithCleanups -> CXXFunctionalCast -> CXXBindTemporary ->
  // CXXConstruct -> CXXStdInitializerList
  std::vector<Test> V4 = std::vector<Test>({{"hi", 1}, {}, {"there", 3}});

  // Returns
  if (i == 1) {
    // Implicit return type
    return {{"hi", 1}, {}, {"there", 3}};
  }
  if (i == 2) {
    // MaterializeTemporary
    return std::vector<Test>{{"hi", 1}, {}, {"there", 3}};
  }
  if (i == 3) {
    // MaterializeTemporary with CTAD
    return std::vector{Test{"hi", 1}, {}, Test{"there", 3}};
  }
  if (i == 4) {
    // Explicit construction of temporary with CTAD
    return std::vector({Test{"hi", 1}, {}, Test{"there", 3}});
  }

  // Generic usage - should become an IIFE since statements cant be neatly added
  // above or below
  return i == 5 ? std::vector<Test>{{}, {}} : std::vector<Test>({{}, {}, {}});
}

void VectorInitializerListCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(
      cxxConstructExpr(
          has(cxxStdInitializerListExpr(
                  hasDescendant(initListExpr().bind("init_list")))
                  .bind("std_init_list")),
          hasDeclaration(
              cxxConstructorDecl(
                  ofClass(cxxRecordDecl(
                      classTemplateSpecializationDecl(
                          hasName("::std::vector"),
                          hasTemplateArgument(
                              0, templateArgument().bind("value_type")))
                          .bind("vector_type"))))
                  .bind("ctor")),
          hasAncestor(exprWithCleanups(
              anyOf(hasParent(varDecl().bind("var_decl")),
                    hasParent(returnStmt(hasAncestor(functionDecl().bind(
                                             "function_that_returns")))
                                  .bind("return"))))))
          .bind("construct_expr"),
      this);
}

void VectorInitializerListCheck::check(const MatchFinder::MatchResult &Result) {
  const ASTContext &Context = *Result.Context;
  const SourceManager &Source = Context.getSourceManager();
  const auto *CIL =
      Result.Nodes.getNodeAs<CXXStdInitializerListExpr>("std_init_list");
  const auto *IL = Result.Nodes.getNodeAs<InitListExpr>("init_list");
  const auto *CE = Result.Nodes.getNodeAs<CXXConstructExpr>("construct_expr");
  // const auto *Ctor = Result.Nodes.getNodeAs<CXXConstructorDecl>("ctor");
  const auto *ValueType =
      Result.Nodes.getNodeAs<TemplateArgument>("value_type");
  const auto *VectorType =
      Result.Nodes.getNodeAs<ClassTemplateSpecializationDecl>("vector_type");
  const auto *VD = Result.Nodes.getNodeAs<VarDecl>("var_decl");
  const auto *RS = Result.Nodes.getNodeAs<ReturnStmt>("return");
  const auto *F = Result.Nodes.getNodeAs<FunctionDecl>("function_that_returns");

  if (ValueType && CE &&
      !ValueType->getAsType().isTriviallyCopyableType(*Result.Context)) {
    if (CIL && IL) {
      std::string TempVectorName = VD ? VD->getNameAsString() : "out";
      std::vector<std::string> EmplaceBackElements;
      for (size_t Idx = 0; Idx < IL->getNumInits(); ++Idx) {
        const auto *Initializer = IL->getInit(Idx);
        EmplaceBackElements.emplace_back(llvm::formatv(
            "{0}.push_back({1})", TempVectorName,
            Lexer::getSourceText(
                CharSourceRange::getTokenRange(Initializer->getSourceRange()),
                Source, Context.getLangOpts())));
      }

      if (EmplaceBackElements.empty())
        return;

      std::string Insertion = llvm::formatv(
          " {0}.reserve({1}); {2}", TempVectorName, EmplaceBackElements.size(),
          llvm::join(EmplaceBackElements, "; "));
      if (VD) {
        // If it's a variable declaration, we can delete the elements from the
        // initializer list and emplace them after!
        diag(CIL->getExprLoc(),
             "(1) Constructing std::vector with an initializer list will "
             "cause elements to be copied.")
            << FixItHint::CreateRemoval(CIL->getSourceRange())
            << FixItHint::CreateInsertion(VD->getEndLoc(), Insertion);
        return;
      }

      if (RS && F) {
        // If it's a return statement, we can delete the elements from the
        // initializer list and construct a temporary vector above (thanks to
        // NRVO)
        Insertion = llvm::formatv(
            " {0} {1};{2}", F->getReturnType().getCanonicalType().getAsString(),
            TempVectorName, Insertion);
        diag(CIL->getExprLoc(),
             "(2) Constructing std::vector with an initializer list will "
             "cause elements to be copied.")
            << FixItHint::CreateRemoval(RS->getSourceRange())
            << FixItHint::CreateInsertion(
                   RS->getBeginLoc().getLocWithOffset(-1), Insertion);
        return;
      }

      // For all other cases, use an immediately-invoked lambda for generality

      std::vector<std::string> TemplateArgumentsAsStrings;
      for (const auto &TArg : VectorType->getTemplateArgs().asArray()) {
        TemplateArgumentsAsStrings.emplace_back(TArg.getAsType().getAsString());
      }

      std::string Replacement = llvm::formatv(
          "[&]{ {0} {1};{2}; return {1}; }()",
          VectorType->getQualifiedNameAsString(), TempVectorName, Insertion);
      diag(CIL->getExprLoc(),
           "(3) Constructing std::vector with an initializer list will cause "
           "elements to be copied.")
          << FixItHint::CreateReplacement(CIL->getSourceRange(), Replacement);
    }
  }
}

} // namespace clang::tidy::performance
