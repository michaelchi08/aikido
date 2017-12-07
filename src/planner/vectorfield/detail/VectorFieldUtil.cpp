#include <Eigen/Geometry>
#include <aikido/common/algorithm.hpp>
#include <aikido/planner/vectorfield/detail/VectorFieldUtil.hpp>
#include <aikido/trajectory/Spline.hpp>

namespace aikido {
namespace planner {
namespace vectorfield {

//==============================================================================
DesiredTwistFunction::DesiredTwistFunction(
    const Twist& twist, const Jacobian& jacobian)
  : dart::optimizer::Function("DesiredTwistFunction")
  , mTwist(twist)
  , mJacobian(jacobian)
{
  // Do nothing
}

//==============================================================================
double DesiredTwistFunction::eval(const Eigen::VectorXd& qd)
{
  return 0.5 * (mJacobian * qd - mTwist).squaredNorm();
}

//==============================================================================
void DesiredTwistFunction::evalGradient(
    const Eigen::VectorXd& qd, Eigen::Map<Eigen::VectorXd> grad)
{
  grad = mJacobian.transpose() * (mJacobian * qd - mTwist);
}

//==============================================================================
std::unique_ptr<aikido::trajectory::Spline> convertToSpline(
    const std::vector<Knot>& knots,
    aikido::statespace::StateSpacePtr stateSpace)
{
  using dart::common::make_unique;

  std::size_t dimension = stateSpace->getDimension();
  auto outputTrajectory = make_unique<aikido::trajectory::Spline>(stateSpace);

  using CubicSplineProblem = aikido::common::
      SplineProblem<double, int, 2, Eigen::Dynamic, Eigen::Dynamic>;

  const Eigen::VectorXd zeroPosition = Eigen::VectorXd::Zero(dimension);
  auto currState = stateSpace->createState();
  for (std::size_t iknot = 0; iknot < knots.size() - 1; ++iknot)
  {
    const double segmentDuration = knots[iknot + 1].mT - knots[iknot].mT;
    Eigen::VectorXd currentPosition = knots[iknot].mPositions;
    Eigen::VectorXd nextPosition = knots[iknot + 1].mPositions;

    CubicSplineProblem problem(
        Eigen::Vector2d{0., segmentDuration}, 2, dimension);
    problem.addConstantConstraint(0, 0, zeroPosition);
    problem.addConstantConstraint(1, 0, nextPosition - currentPosition);
    const auto solution = problem.fit();
    const auto coefficients = solution.getCoefficients().front();

    stateSpace->expMap(currentPosition, currState);
    outputTrajectory->addSegment(coefficients, segmentDuration, currState);
  }
  return outputTrajectory;
}

//==============================================================================
bool computeJointVelocityFromTwist(
    Eigen::VectorXd& jointVelocity,
    const Eigen::Vector6d& desiredTwist,
    const aikido::statespace::dart::MetaSkeletonStateSpacePtr stateSpace,
    const dart::dynamics::BodyNodePtr bodyNode,
    double jointLimitPadding,
    const Eigen::VectorXd& jointVelocityLowerLimits,
    const Eigen::VectorXd& jointVelocityUpperLimits,
    bool jointVelocityLimited,
    double stepSize)
{
  using dart::math::Jacobian;
  using dart::optimizer::Problem;
  using dart::optimizer::Solver;
  using Eigen::VectorXd;

  const dart::dynamics::MetaSkeletonPtr skeleton
      = stateSpace->getMetaSkeleton();
  // Use LBFGS to find joint angles that won't violate the joint limits.
  const Jacobian jacobian = skeleton->getWorldJacobian(bodyNode);

  const std::size_t numDofs = skeleton->getNumDofs();

  jointVelocity = Eigen::VectorXd::Zero(numDofs);
  VectorXd positions = skeleton->getPositions();
  VectorXd initialGuess = skeleton->getVelocities();
  VectorXd positionLowerLimits = skeleton->getPositionLowerLimits();
  VectorXd positionUpperLimits = skeleton->getPositionUpperLimits();
  VectorXd velocityLowerLimits = jointVelocityLowerLimits;
  VectorXd velocityUpperLimits = jointVelocityUpperLimits;

  auto currentState = stateSpace->createState();
  stateSpace->convertPositionsToState(positions, currentState);

  const auto problem = std::make_shared<Problem>(numDofs);
  if (jointVelocityLimited)
  {
    for (std::size_t i = 0; i < numDofs; ++i)
    {
      const double position = positions[i];
      const double positionLowerLimit = positionLowerLimits[i];
      const double positionUpperLimit = positionUpperLimits[i];
      const double velocityLowerLimit = velocityLowerLimits[i];
      const double velocityUpperLimit = velocityUpperLimits[i];

      if (position + stepSize * velocityLowerLimit
          <= positionLowerLimit + jointLimitPadding)
      {
        velocityLowerLimits[i] = 0.0;
      }

      if (position + stepSize * velocityUpperLimit
          >= positionUpperLimit - jointLimitPadding)
      {
        velocityUpperLimits[i] = 0.0;
      }

      initialGuess[i] = common::clamp(
          initialGuess[i], velocityLowerLimits[i], velocityUpperLimits[i]);
    }

    problem->setLowerBounds(velocityLowerLimits);
    problem->setUpperBounds(velocityUpperLimits);
  }
  problem->setInitialGuess(initialGuess);
  problem->setObjective(
      std::make_shared<DesiredTwistFunction>(desiredTwist, jacobian));

  dart::optimizer::NloptSolver solver(problem, nlopt::LD_LBFGS);
  if (!solver.solve())
  {
    return false;
  }

  jointVelocity = problem->getOptimalSolution();
  return true;
}

//==============================================================================
Eigen::Vector6d computeGeodesicTwist(
    const Eigen::Isometry3d& fromTrans, const Eigen::Isometry3d& toTrans)
{
  using dart::math::logMap;
  Eigen::Isometry3d relativeTrans = fromTrans.inverse() * toTrans;
  Eigen::Vector3d relativeTranslation
      = fromTrans.linear() * relativeTrans.translation();
  Eigen::Vector3d axisAngles = logMap(relativeTrans.linear());
  Eigen::Vector3d relativeAngles = fromTrans.linear() * axisAngles;
  Eigen::Vector6d geodesicTwist;
  geodesicTwist << relativeAngles, relativeTranslation;
  return geodesicTwist;
}

//==============================================================================
Eigen::Vector4d computeGeodesicError(
    const Eigen::Isometry3d& fromTrans, const Eigen::Isometry3d& toTrans)
{
  using dart::math::logMap;
  Eigen::Isometry3d relativeTrans = fromTrans.inverse() * toTrans;
  Eigen::Vector3d relativeTranslation
      = fromTrans.linear() * relativeTrans.translation();
  Eigen::Vector3d axisAngles = logMap(relativeTrans.linear());
  Eigen::Vector4d geodesicError;
  geodesicError << axisAngles.norm(), relativeTranslation;
  return geodesicError;
}

//==============================================================================
double computeGeodesicDistance(
    const Eigen::Isometry3d& fromTrans,
    const Eigen::Isometry3d& toTrans,
    double r)
{
  Eigen::Vector4d error = computeGeodesicError(fromTrans, toTrans);
  error[0] = r * error[0];
  return error.norm();
}

} // namespace vectorfield
} // namespace planner
} // namespace aikido