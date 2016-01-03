#include "ao/tree/tree.hpp"
#include "ao/eval/evaluator.hpp"

////////////////////////////////////////////////////////////////////////////////

Evaluator::Evaluator(const Tree* tree)
{
    std::unordered_map<const Atom*, Clause*> clauses;

    // Count up the number of Atoms in the Tree and allocate space for them
    size_t count = 3                // X, Y, Z
         + tree->matrix.size()      // Transform matrix
         + tree->constants.size();  // Constants
    for (auto r : tree->rows)
    {
        count += r.size();
    }
    data = static_cast<Clause*>(malloc(sizeof(Clause) * count));
    ptr = data;

    // Load constants into the array first
    for (auto m : tree->constants)
    {
        constants.push_back(newClause(m, clauses));
    }

    // Create base clauses X, Y, Z
    X = newClause(tree->X, clauses);
    Y = newClause(tree->Y, clauses);
    Z = newClause(tree->Z, clauses);

    // Create matrix clauses
    assert(tree->matrix.size() == matrix.size());
    for (size_t i=0; i < tree->matrix.size(); ++i)
    {
        matrix[i] = newClause(tree->matrix[i], clauses);
    }

    // Finally, create the rest of the Tree's clauses
    for (auto r : tree->rows)
    {
        rows.push_back(Row());
        for (auto m : r)
        {
            rows.back().push_back(newClause(m, clauses));
        }
        rows.back().setSize();
    }

    assert(clauses[tree->root]);
    root = clauses[tree->root];
}

Evaluator::~Evaluator()
{
    free(data);
}

////////////////////////////////////////////////////////////////////////////////

void Evaluator::setMatrix(const glm::mat4& m)
{
    size_t index = 0;
    for (int i=0; i < 3; ++i)
    {
        for (int j=0; j < 4; ++j)
        {
            assert(matrix[index]->op == OP_MUTABLE);
            matrix[index++]->mutable_value = m[j][i];
        }
    }

    for (auto m : matrix)
    {
        m->result.fill(m->mutable_value);
    }
}

////////////////////////////////////////////////////////////////////////////////

void Evaluator::push()
{
    // Walk up the tree, marking every atom with ATOM_FLAG_IGNORED
    for (const auto& row : rows)
    {
        for (size_t i=0; i < row.active; ++i)
        {
            row[i]->setFlag(CLAUSE_FLAG_IGNORED);
        }
    }

    // Clear the IGNORED flag on the root
    root->clearFlag(CLAUSE_FLAG_IGNORED);

    // Walk down the tree, clearing IGNORED flags as appropriate
    // and disabling atoms that still have IGNORED flags set.
    for (auto itr = rows.rbegin(); itr != rows.rend(); ++itr)
    {
        itr->push();
    }
}

void Evaluator::pop()
{
    for (auto& row : rows)
    {
        row.pop();
    }
}

////////////////////////////////////////////////////////////////////////////////

const double* Evaluator::eval(const Region& r)
{
    assert(r.voxels() <= Result::count<double>());

    size_t index = 0;

    // Flatten the region in a particular order
    // (which needs to be obeyed by anything unflattening results)
    REGION_ITERATE_XYZ(r)
    {
        X->result.set(r.X.pos(i), index);
        Y->result.set(r.Y.pos(j), index);
        Z->result.set(r.Z.pos(r.Z.size - k - 1), index);
        index++;
    }

    return evalCore<double>(r.voxels());
}

////////////////////////////////////////////////////////////////////////////////

Clause* Evaluator::newClause(const Atom* m,
                             std::unordered_map<const Atom*, Clause*>& clauses)
{
    return new (ptr++) Clause(m, clauses);
}