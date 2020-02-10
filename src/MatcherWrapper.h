#pragma once

#include <clang/ASTMatchers/ASTMatchersInternal.h>
#include <clang/ASTMatchers/ASTMatchersMacros.h>

namespace match
{
template <typename T>
class WrapperMatcher
    : public ::clang::ast_matchers::internal::MatcherInterface<T>
{
  public:
    WrapperMatcher(::clang::ast_matchers::internal::Matcher<T>& matcher,
                   bool dump = false)
        : realMatcher{matcher}, dump{dump}
    {
    }

    bool matches(const T& Node,
                 ::clang::ast_matchers::internal::ASTMatchFinder* Finder,
                 ::clang::ast_matchers::internal::BoundNodesTreeBuilder*
                     Builder) const override
    {
        if (dump)
            Node.dump();
        return realMatcher.matches(Node, Finder, Builder);
    }

  private:
    ::clang::ast_matchers::internal::Matcher<T>& realMatcher;
    bool dump;
};

template <typename T>
::clang::ast_matchers::internal::Matcher<T> ref(
    ::clang::ast_matchers::internal::Matcher<T>& m)
{
    return ::clang::ast_matchers::internal::makeMatcher(new WrapperMatcher(m));
}

template <typename T>
::clang::ast_matchers::internal::Matcher<T> dumpped(
    ::clang::ast_matchers::internal::Matcher<T>& m)
{
    return ::clang::ast_matchers::internal::makeMatcher(
        new WrapperMatcher(m, true));
}
} // namespace match
