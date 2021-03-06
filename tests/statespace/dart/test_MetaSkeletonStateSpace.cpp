#include <dart/dynamics/dynamics.hpp>
#include <gtest/gtest.h>
#include <aikido/statespace/CartesianProduct.hpp>
#include <aikido/statespace/Rn.hpp>
#include <aikido/statespace/SE2.hpp>
#include <aikido/statespace/SE3.hpp>
#include <aikido/statespace/SO2.hpp>
#include <aikido/statespace/SO3.hpp>
#include <aikido/statespace/dart/MetaSkeletonStateSpace.hpp>

using Eigen::Isometry3d;
using Eigen::Vector3d;
using dart::dynamics::Skeleton;
using dart::dynamics::RevoluteJoint;
using dart::dynamics::PrismaticJoint;
using dart::dynamics::TranslationalJoint;
using dart::dynamics::FreeJoint;
using aikido::statespace::dart::MetaSkeletonStateSpace;
using aikido::statespace::R1;
using aikido::statespace::R3;
using aikido::statespace::SO2;
using aikido::statespace::SE3;

static Eigen::Matrix<double, 1, 1> make_scalar(double _value)
{
  Eigen::Matrix<double, 1, 1> matrix;
  matrix(0, 0) = _value;
  return matrix;
}

TEST(MetaSkeletonStateSpace, RevoluteJoint_WithoutBounds_CreatesSO2)
{
  auto skeleton = Skeleton::create();
  skeleton->createJointAndBodyNodePair<RevoluteJoint>();

  MetaSkeletonStateSpace space(skeleton);
  ASSERT_EQ(1, space.getNumSubspaces());

  auto state = space.createState();
  auto substate = state.getSubStateHandle<SO2>(0);

  skeleton->setPosition(0, 5.);
  space.getState(state);
  EXPECT_DOUBLE_EQ(5., substate.getAngle());

  substate.setAngle(6.);
  space.setState(state);
  EXPECT_DOUBLE_EQ(6., skeleton->getPosition(0));
}

TEST(MetaSkeletonStateSpace, RevoluteJoint_WithBounds_CreatesRealVector)
{
  auto skeleton = Skeleton::create();
  auto joint = skeleton->createJointAndBodyNodePair<RevoluteJoint>().first;
  joint->setPositionLowerLimit(0, -1.);

  MetaSkeletonStateSpace space(skeleton);
  ASSERT_EQ(1, space.getNumSubspaces());

  auto subspace = space.getSubspace<R1>(0);
  ASSERT_EQ(1, subspace->getDimension());

  auto state = space.createState();
  auto substate = state.getSubStateHandle<R1>(0);

  skeleton->setPosition(0, 5.);
  space.getState(state);
  EXPECT_DOUBLE_EQ(5., substate.getValue()[0]);

  substate.setValue(make_scalar(6.));
  space.setState(state);
  EXPECT_DOUBLE_EQ(6., skeleton->getPosition(0));
}

TEST(MetaSkeletonStateSpace, PrismaticJoint_CreatesRealVector)
{
  auto skeleton = Skeleton::create();
  skeleton->createJointAndBodyNodePair<PrismaticJoint>();

  MetaSkeletonStateSpace space(skeleton);
  ASSERT_EQ(1, space.getNumSubspaces());

  auto subspace = space.getSubspace<R1>(0);
  ASSERT_EQ(1, subspace->getDimension());

  auto state = space.createState();
  auto substate = state.getSubStateHandle<R1>(0);

  skeleton->setPosition(0, 5.);
  space.getState(state);
  EXPECT_DOUBLE_EQ(5., substate.getValue()[0]);

  substate.setValue(make_scalar(6.));
  space.setState(state);
  EXPECT_DOUBLE_EQ(6., skeleton->getPosition(0));
}

TEST(MetaSkeletonStateSpace, TranslationalJoint_CreatesRealVector)
{
  Vector3d value1(1., 2., 3.);
  Vector3d value2(4., 5., 6.);

  auto skeleton = Skeleton::create();
  skeleton->createJointAndBodyNodePair<TranslationalJoint>();

  MetaSkeletonStateSpace space(skeleton);
  ASSERT_EQ(1, space.getNumSubspaces());

  auto subspace = space.getSubspace<R3>(0);
  ASSERT_EQ(3, subspace->getDimension());

  auto state = space.createState();
  auto substate = state.getSubStateHandle<R3>(0);

  skeleton->setPositions(value1);
  space.getState(state);
  EXPECT_TRUE(value1.isApprox(substate.getValue()));

  substate.setValue(value2);
  space.setState(state);
  EXPECT_TRUE(value2.isApprox(skeleton->getPositions()));
}

TEST(MetaSkeletonStateSpace, FreeJoint_CreatesSE3)
{
  Isometry3d value1 = Isometry3d::Identity();
  value1.rotate(Eigen::AngleAxisd(M_PI_2, Vector3d::UnitZ()));
  value1.pretranslate(Vector3d(1., 2., 3.));

  Isometry3d value2 = Isometry3d::Identity();
  value2.rotate(Eigen::AngleAxisd(3 * M_PI_2, Vector3d::UnitZ()));
  value2.pretranslate(Vector3d(4., 5., 6.));

  auto skeleton = Skeleton::create();
  skeleton->createJointAndBodyNodePair<FreeJoint>();

  MetaSkeletonStateSpace space(skeleton);
  ASSERT_EQ(1, space.getNumSubspaces());

  auto state = space.createState();
  auto substate = state.getSubStateHandle<SE3>(0);

  skeleton->setPositions(FreeJoint::convertToPositions(value1));
  space.getState(state);
  EXPECT_TRUE(value1.isApprox(substate.getIsometry()));

  substate.setIsometry(value2);
  space.setState(state);
  EXPECT_TRUE(
      value2.isApprox(FreeJoint::convertToTransform(skeleton->getPositions())));
}

TEST(MetaSkeletonStateSpace, MultipleJoints_CreatesCartesianProduct)
{
  Vector3d value1(2., 3., 4.);
  Vector3d value2(6., 7., 8.);

  auto skeleton = Skeleton::create();
  auto joint1 = skeleton->createJointAndBodyNodePair<RevoluteJoint>().first;
  auto joint2
      = skeleton->createJointAndBodyNodePair<TranslationalJoint>().first;

  MetaSkeletonStateSpace space(skeleton);
  ASSERT_EQ(2, space.getNumSubspaces());

  auto state = space.createState();
  auto substate1 = state.getSubStateHandle<SO2>(0);
  auto substate2 = state.getSubStateHandle<R3>(1);

  joint1->setPosition(0, 1.);
  joint2->setPositions(value1);
  space.getState(state);
  EXPECT_EQ(1., substate1.getAngle());
  EXPECT_TRUE(value1.isApprox(value1));

  substate1.setAngle(5.);
  substate2.setValue(value2);
  space.setState(state);
  EXPECT_EQ(5., substate1.getAngle());
  EXPECT_TRUE(value2.isApprox(substate2.getValue()));
}
