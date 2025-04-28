#include <gringo/output/literal.hh>
#include <gringo/output/statements.hh>
#include <gringo/output/theory.hh>

namespace Gringo { namespace Output {

void PrintPlain::printElem(Potassco::Id_t x) {
    domain.theory().printElem(stream, x, [this](std::ostream &, LiteralId const &lit){ call(domain, lit, &Literal::printPlain, *this); });
}

void PrintPlain::printTerm(Potassco::Id_t x) {
    domain.theory().printTerm(stream, x);
}

bool isTrueClause(DomainData &data, LitVec &lits, IsTrueLookup const &lookup) {
    for (auto &lit : lits) {
        if (!call(data, lit, &Literal::isTrue, lookup)) {
            return false;
        }
    }
    return true;
}

bool Literal::isAtomFromPreviousStep() const {
    return false;
}

bool Literal::isHeadAtom() const {
    return false;
}

bool Literal::isIncomplete() const {
    return false;
}

std::pair<LiteralId,bool> Literal::delayedLit() {
    throw std::logic_error("Literal::hasDelayedOffset: not implemented");
}

bool Literal::needsSemicolon() const {
    return false;
}

bool Literal::isPositive() const {
    return true;
}

LiteralId Literal::simplify(Mappings &mappings, AssignmentLookup const &lookup) const {
    throw std::logic_error("Literal::simplify: not implemented");
}

bool Literal::isTrue(IsTrueLookup const &lookup) const {
    throw std::logic_error("Literal::isTrue: not implemented");
}

} } // namespace Output Gringo
