//===--- VectorPessimizationCheck.cpp - clang-tidy ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "VectorPessimizationCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Type.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/DiagnosticIDs.h"

using namespace clang::ast_matchers;

namespace clang::tidy::performance {
namespace {
static const char VectorTokenId[] = "VectorTokenId";
static const char ValueTypeId[] = "ValueTypeId";
static constexpr size_t MaxRecursionDepth = 3U;

bool hasNothrowMoveConstructor(const QualType &Type,
                               utils::ExceptionSpecAnalyzer &SpecAnalyzer) {
  auto *Record = Type->getAsCXXRecordDecl();
  if (!Record || !Record->hasDefinition())
    return false;
  for (const auto *Constructor : Record->ctors()) {
    if (Constructor->isMoveConstructor())
      return SpecAnalyzer.analyze(Constructor) !=
             utils::ExceptionSpecAnalyzer::State::Throwing;
  }
  return false;
}

const CXXConstructorDecl *getThrowingUserDefinedMoveConstructor(
    const CXXRecordDecl *Record, utils::ExceptionSpecAnalyzer &SpecAnalyzer) {
  for (auto *Constructor : Record->ctors()) {
    if (Constructor->isMoveConstructor() && Constructor->isUserProvided())
      return SpecAnalyzer.analyze(Constructor) ==
                     utils::ExceptionSpecAnalyzer::State::Throwing
                 ? Constructor
                 : nullptr;
  }
  return nullptr;
}

bool isTriviallyCopyable(const QualType &Type) {
  if (Type->isBuiltinType() || Type->isEnumeralType())
    return true;
  auto *Record = Type->getAsCXXRecordDecl();
  if (!Record || !Record->hasDefinition())
    return false;
  return Record->isTriviallyCopyable();
}

bool willDegradeToCopy(const QualType &Type,
                       utils::ExceptionSpecAnalyzer &SpecAnalyzer) {
  return !isTriviallyCopyable(Type) &&
         !hasNothrowMoveConstructor(Type, SpecAnalyzer);
}

const FieldDecl *
getFirstThrowingDataMember(const CXXRecordDecl *Record,
                           utils::ExceptionSpecAnalyzer &SpecAnalyzer) {
  for (auto *Field : Record->fields()) {
    if (Field && Field->isCXXClassMember() &&
        willDegradeToCopy(Field->getType(), SpecAnalyzer))
      return Field;
  }
  return nullptr;
}

std::optional<CXXBaseSpecifier>
getFirstThrowingBaseClass(const CXXRecordDecl *Record,
                          utils::ExceptionSpecAnalyzer &SpecAnalyzer) {
  for (auto BaseClass : Record->bases()) {
    if (!BaseClass.isVirtual() &&
        willDegradeToCopy(BaseClass.getType(), SpecAnalyzer))
      return BaseClass;
  }
  return std::nullopt;
}

} // namespace

void VectorPessimizationCheck::recursivelyCheckMembers(
    const MatchFinder::MatchResult &Result, const QualType &Type,
    const size_t RecursionDepth) {
  auto *Record = Type->getAsCXXRecordDecl();
  if (!Record || !Record->hasDefinition())
    return;
  diag(Record->getLocation(), "'%0' defined here", DiagnosticIDs::Note)
      << QualType{Record->getTypeForDecl(), 0}.getAsString();
  if (const auto *MoveCTOR =
          getThrowingUserDefinedMoveConstructor(Record, SpecAnalyzer);
      MoveCTOR != nullptr) {
    diag(MoveCTOR->getLocation(), "throwing move constructor declared here",
         DiagnosticIDs::Note);
  } else if (const auto *ThrowingDataMember =
                 getFirstThrowingDataMember(Record, SpecAnalyzer);
             ThrowingDataMember != nullptr) {
    QualType MemberType = ThrowingDataMember->getType();
    MemberType.removeLocalFastQualifiers();
    diag(ThrowingDataMember->getLocation(),
         "because the move constructor of '%0' may throw", DiagnosticIDs::Note)
        << MemberType.getAsString();
    if (RecursionDepth >= MaxRecursionDepth)
      return;
    recursivelyCheckMembers(Result, ThrowingDataMember->getType(),
                            RecursionDepth + 1U);
  } else if (const auto ThrowingBaseClass =
                 getFirstThrowingBaseClass(Record, SpecAnalyzer);
             ThrowingBaseClass.has_value()) {
    diag(ThrowingBaseClass->getBeginLoc(),
         "because the move constructor of '%0' may throw", DiagnosticIDs::Note)
        << ThrowingBaseClass->getType().getAsString();
    if (RecursionDepth >= MaxRecursionDepth)
      return;
    recursivelyCheckMembers(Result, ThrowingBaseClass->getType(),
                            RecursionDepth + 1U);
  }
}

void VectorPessimizationCheck::registerMatchers(MatchFinder *Finder) {
  auto VectorDeclMatcher = recordDecl(classTemplateSpecializationDecl(
      hasName("::std::vector"),
      hasTemplateArgument(
          0, templateArgument(refersToType(qualType().bind(ValueTypeId))))));

  Finder->addMatcher(loc(qualType(qualType(hasDeclaration(VectorDeclMatcher)),
                                  // Skip elaboratedType() as the named
                                  // type will match soon thereafter.
                                  unless(elaboratedType())))
                         .bind(VectorTokenId),
                     this);
}

void VectorPessimizationCheck::check(const MatchFinder::MatchResult &Result) {
  // FIXME: Add callback implementation.
  const auto *VectorType = Result.Nodes.getNodeAs<TypeLoc>(VectorTokenId);
  const auto *ValueType = Result.Nodes.getNodeAs<QualType>(ValueTypeId);
  if (!VectorType || !ValueType ||
      !willDegradeToCopy(*ValueType, SpecAnalyzer)) {
    return;
  }

  diag(VectorType->getBeginLoc(),
       "'%0' will copy elements on resize instead of moving because the move "
       "constructor of '%1' may throw")
      << VectorType->getType().getAsString() << ValueType->getAsString();

  recursivelyCheckMembers(Result, *ValueType, 1U);
}

} // namespace clang::tidy::performance
