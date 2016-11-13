#include <memory>
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "distribution.h"

using ::testing::ElementsAreArray;
using ::testing::DoubleEq;
using ::testing::StrEq;

class DistributionTest: public ::testing::Test {
public:
    virtual void SetUp() {
        auto f1 = std::make_shared<splittercell::Flock>(std::vector<unsigned int>({0,1}));
        dist = new splittercell::Distribution({f1});
        dist->set_probabilities(0, {0.1, 0.2, 0.1, 0.6});

        auto f2 = std::make_shared<splittercell::Flock>(std::vector<unsigned int>({0,1}), std::vector<unsigned int>({2}));
        f2->set_probabilities({0.1, 0.0, 0.0, 0.2, 0.5, 0.0, 0.1, 0.1});
        conditioned = new splittercell::Distribution({f2});

        auto f3 = std::make_shared<splittercell::Flock>(std::vector<unsigned int>({2,3}), std::vector<unsigned int>({4}));
        auto f4 = std::make_shared<splittercell::Flock>(std::vector<unsigned int>({0,1}), std::vector<unsigned int>({2}));
        auto f5 = std::make_shared<splittercell::Flock>(std::vector<unsigned int>({4}));
        threeflocks = new splittercell::Distribution({f4, f3, f5});
        threeflocks->set_probabilities(0, {0.2, 0.0, 0.0, 0.8, 0.7, 0.0, 0.15, 0.15});
        threeflocks->set_probabilities(1, {0.2, 0.0, 0.0, 0.8, 0.7, 0.0, 0.15, 0.15});
    }

    virtual void TearDown() {
        delete dist;
        delete conditioned;
        delete threeflocks;
    }

    splittercell::Distribution *dist, *conditioned, *threeflocks;
};

TEST_F(DistributionTest, Refine1A) {
    EXPECT_THAT(dist->to_str(), StrEq("0.1 0.2 0.1 0.6"));
    dist->refine(0, true, 1.0);
    EXPECT_THAT(dist->to_str(), StrEq("0 0.3 0 0.7"));

    EXPECT_THAT(conditioned->to_str(), StrEq("0.1 0 0 0.2 0.5 0 0.1 0.1"));
    conditioned->refine(0, true, 1.0);
    EXPECT_THAT(conditioned->to_str(), StrEq("0 0.1 0 0.2 0 0.5 0 0.2"));
}

TEST_F(DistributionTest, Refine1NotA) {
    EXPECT_THAT(dist->to_str(), StrEq("0.1 0.2 0.1 0.6"));
    dist->refine(0, false, 1.0);
    EXPECT_THAT(dist->to_str(), StrEq("0.3 0 0.7 0"));

    EXPECT_THAT(conditioned->to_str(), StrEq("0.1 0 0 0.2 0.5 0 0.1 0.1"));
    conditioned->refine(0, false, 1.0);
    EXPECT_THAT(conditioned->to_str(), StrEq("0.1 0 0.2 0 0.5 0 0.2 0"));
}

TEST_F(DistributionTest, Refine75A) {
    EXPECT_THAT(dist->to_str(), StrEq("0.1 0.2 0.1 0.6"));
    dist->refine(0, true, 0.75);
    EXPECT_THAT(dist->to_str(), StrEq("0.025 0.275 0.025 0.675"));
}

TEST_F(DistributionTest, Refine1B) {
    EXPECT_THAT(dist->to_str(), StrEq("0.1 0.2 0.1 0.6"));
    dist->refine(1, true, 1.0);
    EXPECT_THAT(dist->to_str(), StrEq("0 0 0.2 0.8"));

    EXPECT_THAT(conditioned->to_str(), StrEq("0.1 0 0 0.2 0.5 0 0.1 0.1"));
    conditioned->refine(1, true, 1.0);
    EXPECT_THAT(conditioned->to_str(), StrEq("0 0 0.1 0.2 0 0 0.6 0.1"));
}

TEST_F(DistributionTest, MarginalizeA) {
    EXPECT_THAT(dist->to_str(), StrEq("0.1 0.2 0.1 0.6"));
    auto m = dist->marginalize(0, {0, 4})->distribution(); //To test that non included arguments are ignored
    EXPECT_THAT(m, ElementsAreArray({DoubleEq(0.2), DoubleEq(0.8)}));

    EXPECT_THAT(conditioned->to_str(), StrEq("0.1 0 0 0.2 0.5 0 0.1 0.1"));
    m = conditioned->marginalize(0, {0})->distribution();
    EXPECT_THAT(m, ElementsAreArray({DoubleEq(0.1), DoubleEq(0.2), DoubleEq(0.6), DoubleEq(0.1)}));
}

TEST_F(DistributionTest, MarginalizeB) {
    EXPECT_THAT(dist->to_str(), StrEq("0.1 0.2 0.1 0.6"));
    auto m = dist->marginalize(0, {1})->distribution();
    EXPECT_THAT(m, ElementsAreArray({DoubleEq(0.3), DoubleEq(0.7)}));

    EXPECT_THAT(conditioned->to_str(), StrEq("0.1 0 0 0.2 0.5 0 0.1 0.1"));
    m = conditioned->marginalize(0, {1})->distribution();
    EXPECT_THAT(m, ElementsAreArray({DoubleEq(0.1), DoubleEq(0.2), DoubleEq(0.5), DoubleEq(0.2)}));
}

TEST_F(DistributionTest, MarginalizeAB) {
    EXPECT_THAT(dist->to_str(), StrEq("0.1 0.2 0.1 0.6"));
    auto m = dist->marginalize(0, {0,1})->distribution();
    EXPECT_THAT(m, ElementsAreArray({DoubleEq(0.1), DoubleEq(0.2), DoubleEq(0.1), DoubleEq(0.6)}));

    EXPECT_THAT(conditioned->to_str(), StrEq("0.1 0 0 0.2 0.5 0 0.1 0.1"));
}

TEST_F(DistributionTest, Combine) {
    auto m = threeflocks->flock(0)->combine(threeflocks->flock(1));
    EXPECT_THAT(m->distribution(), ElementsAreArray({DoubleEq(0.04), DoubleEq(0.0), DoubleEq(0.0), DoubleEq(0.16),
                                     DoubleEq(0.0), DoubleEq(0.0), DoubleEq(0.0), DoubleEq(0),
                                     DoubleEq(0.0), DoubleEq(0.0), DoubleEq(0.0), DoubleEq(0.0),
                                     DoubleEq(0.56), DoubleEq(0.0), DoubleEq(0.12), DoubleEq(0.12),
                                     DoubleEq(0.14), DoubleEq(0.0), DoubleEq(0.0), DoubleEq(0.56),
                                     DoubleEq(0.0), DoubleEq(0.0), DoubleEq(0.0), DoubleEq(0.0),
                                     DoubleEq(0.03), DoubleEq(0.0), DoubleEq(0.0), DoubleEq(0.12),
                                     DoubleEq(0.105), DoubleEq(0.0), DoubleEq(0.0225), DoubleEq(0.0225)}));
}

TEST_F(DistributionTest, InitialBeliefA) {
    auto beliefs = (*dist)[{0}];
    EXPECT_THAT(beliefs[0], DoubleEq(0.8));
}

TEST_F(DistributionTest, InitialBeliefB) {
    auto beliefs = (*dist)[{1}];
    EXPECT_THAT(beliefs[1], DoubleEq(0.7));
}

TEST_F(DistributionTest, InitialBeliefAB) {
    auto beliefs = (*dist)[{0,1}];
    EXPECT_THAT(beliefs[0], DoubleEq(0.8));
    EXPECT_THAT(beliefs[1], DoubleEq(0.7));
}

TEST_F(DistributionTest, ThreeSplitA) {
    EXPECT_THAT(threeflocks->operator[]({0})[0], DoubleEq(0.49125));
}

TEST_F(DistributionTest, ThreeSplitD) {
    EXPECT_THAT(threeflocks->operator[]({3})[3], DoubleEq(0.55));
}

TEST_F(DistributionTest, ThreeSplitC) {
    EXPECT_THAT(threeflocks->operator[]({2})[2], DoubleEq(0.475));
}

TEST_F(DistributionTest, ThreeSplitE) {
    EXPECT_THAT(threeflocks->operator[]({4})[4], DoubleEq(0.5));
}

TEST(StressTest, HugeRefinement) {
    std::vector<unsigned int> args;
    for(unsigned int i = 0; i < 25; i++)
        args.push_back(i);
    splittercell::Flock f(args);
    f.refine(0, true, 1);
}

TEST(StressTest, HugeMarginalization) {
    std::vector<unsigned int> args;
    for(unsigned int i = 0; i < 25; i++)
    args.push_back(i);
    splittercell::Flock f(args);
    f.marginalize({0});
}

TEST(StressTest, HugeCombination) {
    std::vector<unsigned int> args1, args2;
    for(unsigned int i = 0; i < 12; i++)
        args1.push_back(i);
    for(unsigned int i = 12; i < 25; i++)
        args2.push_back(i);
    splittercell::Flock f1(args1, std::vector<unsigned int>({12}));
    auto f2 = std::make_shared<splittercell::Flock>(args2);
    f1.combine(f2);
}
