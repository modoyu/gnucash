/********************************************************************
 * test-autoclear.c: test suite for Auto-Clear          	    *
 * Copyright 2020 Cristian Klein <cristian@kleinlabs.eu>            *
 *                                                                  *
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, you can retrieve it from        *
 * https://www.gnu.org/licenses/old-licenses/gpl-2.0.html            *
 * or contact:                                                      *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652       *
 * Boston, MA  02110-1301,  USA       gnu@gnu.org                   *
 ********************************************************************/
#include "config.h"

// GoogleTest is written in C++, however, the function we test in C.
extern "C" {
#include "../gnc-ui-balances.h"
}
#include "Split.h"

#include "gtest/gtest.h"

static const int64_t DENOM = 100; //< Denomerator is always 100 for simplicity.

struct SplitDatum {
    const char *memo;
    gint64 amount; //< Numerator of amount.
    bool cleared;
};

struct Tests {
    gint64 amount;
    const char *expectedErr;
};

struct TestCase {
    std::vector<SplitDatum> splits;
    std::vector<Tests> tests;
};

TestCase easyTestCase = {
    .splits = {
        { "Memo 01", -  8234, true },
        { "Memo 02", -156326, true },
        { "Memo 03", -  4500, true },
        { "Memo 04", -694056, true },
        { "Memo 05", -  7358, true },
        { "Memo 06", - 11700, true },
        { "Memo 07", - 20497, true },
        { "Memo 08", - 11900, true },
        { "Memo 09", -  8275, true },
        { "Memo 10", - 58700, true },
        { "Memo 11", +100000, true },
        { "Memo 12", - 13881, true },
        { "Memo 13", -  5000, true },
        { "Memo 14", +200000, true },
        { "Memo 15", - 16800, true },
        { "Memo 16", -152000, true },
        { "Memo 17", +160000, false },
        { "Memo 18", - 63610, false },
        { "Memo 19", -  2702, false },
        { "Memo 20", - 15400, false },
        { "Memo 21", -  3900, false },
        { "Memo 22", - 22042, false },
        { "Memo 23", -  2900, false },
        { "Memo 24", - 10900, false },
        { "Memo 25", - 44400, false },
        { "Memo 26", -  9200, false },
        { "Memo 27", -  7900, false },
        { "Memo 28", -  1990, false },
        { "Memo 29", -  7901, false },
        { "Memo 30", - 61200, false },
    },
    .tests = {
        { 0, "The selected amount cannot be cleared.", },
        { -869227, "Account is already at Auto-Clear Balance." }, // No splits need to be cleared.
        { -869300, "The selected amount cannot be cleared." },
        { -869230, NULL },
        { -963272, NULL }, // All splits need to be cleared.
    },
};

TestCase ambiguousTestCase = {
    .splits = {
        { "Memo 01", -10, false },
        { "Memo 02", -10, false },
        { "Memo 03", -10, false },
    },
    .tests = {
        { -10, "Cannot uniquely clear splits. Found multiple possibilities." },
        { -20, "Cannot uniquely clear splits. Found multiple possibilities." },

        // Forbid auto-clear to be too smart. We expect the user to manually deal
        // with such situations.
        { -30, "Cannot uniquely clear splits. Found multiple possibilities." },
    },
};

class AutoClearTest : public testing::TestWithParam<TestCase> {
protected:
    std::shared_ptr<QofBook> book_;
    Account *account_; // owned by book_
    TestCase testCase_;

public:
    AutoClearTest() :
        book_(qof_book_new(), qof_book_destroy),
        account_(xaccMallocAccount(book_.get()))
    {
        testCase_ = GetParam();

        xaccAccountSetName(account_, "Test Account");
        xaccAccountBeginEdit(account_);
        for (auto &d : testCase_.splits) {
            Split *split = xaccMallocSplit(book_.get());
            xaccSplitSetMemo(split, d.memo);
            xaccSplitSetAmount(split, gnc_numeric_create(d.amount, DENOM));
            xaccSplitSetReconcile(split, d.cleared ? CREC : NREC);
            xaccSplitSetAccount(split, account_);

            gnc_account_insert_split(account_, split);
        }
        xaccAccountCommitEdit(account_);
    }
};

TEST_P(AutoClearTest, DoesAutoClear) {
    for (auto &t : testCase_.tests) {
        gnc_numeric amount_to_clear = gnc_numeric_create(t.amount, DENOM);
        char *err;

        GList *splits_to_clear = gnc_account_get_autoclear_splits(account_, amount_to_clear, &err);

        // Actually clear splits
        for (GList *node = splits_to_clear; node; node = node->next) {
            Split *split = (Split *)node->data;
            xaccSplitSetReconcile(split, CREC);
        }

        ASSERT_STREQ(err, t.expectedErr);
        if (t.expectedErr == NULL) {
            gnc_numeric c = xaccAccountGetClearedBalance(account_);
            ASSERT_EQ(c.num, t.amount);
            ASSERT_EQ(c.denom, DENOM);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    InstantiationAutoClearTest,
    AutoClearTest,
    testing::Values(
        easyTestCase,
        ambiguousTestCase
    )
);
