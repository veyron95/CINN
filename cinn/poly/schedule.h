#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cinn/common/common.h"
#include "cinn/lang/tensor.h"
#include "cinn/poly/graph.h"
#include "cinn/poly/isl_utils.h"
#include "cinn/poly/map.h"
#include "cinn/poly/stage.h"

namespace cinn {
namespace poly {

struct TimeDim {
  //! time of this dimension.
  int time;
  //! name of this dimension.
  std::string dim;

  TimeDim() = default;
  TimeDim(std::string dim, int time) : dim(std::move(dim)), time(time) {}
};

struct DependFlow {
  //! Map from the depended Element.id to the level.
  std::unordered_map<std::string, int> depend_level;
};

class ScheduleGraphNode;
struct ScheduleGraph : public common::Graph {};

/**
 * ISL schedule map with time space, used to generate the final schedule.
 */
struct TimeSchedule {
  TimeSchedule(const std::string &id, const std::vector<std::string> &dims);

  void ResizeTimeSpace(int size) { time_dims.resize(size); }

  //! Schedule this after \p other in \p level.
  void OrderAfter(const TimeSchedule &other, int level);

  size_t space_size() const { return time_dims.size(); }

  const std::string &id() const;

  //! Get the isl map.
  isl::map to_isl(isl::ctx ctx) const;

  //! ISL range format, such as '[dup, t0, t1]: dup=0 and t0=0 and t1=i]'
  std::string __str__() const;

  //! Get the axis names with the original dimension names and faked time dimensions.
  std::vector<std::string> final_axis_names() const;

  std::vector<std::string> domain_dims;
  int duplicate_id{};
  std::vector<TimeDim> time_dims;

 private:
  std::string id_;
};

/**
 * The base class for all the Scheduler.
 */
class SchedulerBase {
 protected:
  /**
   * Register an Element to the scheduler.
   */
  void AddStage(const Stage &x);

  /**
   * Finalize the registration.
   */
  void FinishStageAdd();

  /**
   * Tell whether the registration is finalized.
   */
  bool finalized() const { return registration_finalized_; }
  int space_size() const { return space_size_; }

 protected:
  /**
   * The polyhedral schedule, any schedule is performed on it.
   * We use the time-space map to record the schedule infomation, the format is borrowed from Tiramisu project:
   * [time,dim,time,dim,time,dim ...]
   */
  int space_size_{0};
  mutable isl::ctx ctx_{Context::Global().isl_ctx()};
  mutable ScheduleGraph schedule_graph_;
  // Record the longest dimensions(of some stage) to be the final detailed dimension names. It might be used for ISL AST
  // to set iterator names and generate readable code.
  mutable std::vector<std::string> detailed_dimension_names_;

 private:
  bool registration_finalized_{false};
};

/**
 * The NaiveScheduler just schedule each noninlined Tensor as a unique group. Only the `compute_at` will make two tensor
 * in the same group. It is simple and robust.
 */
class NaiveScheduler : public SchedulerBase {
 public:
  NaiveScheduler() = default;
  explicit NaiveScheduler(const std::vector<Stage *> &stages);

  std::map<std::string, isl::map> BuildSchedule() const;

 private:
  void PartitionGroups();

 private:
  common::Graph *graph_{};
  std::vector<detail::Group> groups_;
  mutable std::vector<std::string> detailed_dimension_names_;
};

/**
 * Record the schedule information for several groups.
 */
class Schedule {
 public:
  /*
   * Constructor.
   * @param graph A graph consisted of DataFlowGraphNodes
   */
  explicit Schedule(common::Graph *graph) : graph_(graph) {
    PartitionGroups();
    // ScheduleEachGroup();
  }

  //! Generated groups.
  std::vector<detail::Group> &gened_groups() { return groups_; }
  const std::vector<detail::Group> &gened_groups() const { return groups_; }

 private:
  //! Partition the graph into several groups(sub-graph).
  void PartitionGroups();

  //! Schedule a single group.
  void ScheduleGroup(detail::Group *group);

  void ScheduleEachGroup();

 private:
  common::Graph *graph_{};
  std::vector<detail::Group> groups_;
};

/**
 * Create the schedule from a tensor, it will retrive the dependency tensors.
 */
std::unique_ptr<Schedule> CreateSchedule(const ir::Tensor &tensor);

/**
 * Get the schedule given some stages.
 * A Schedule defines the execution order of the stages follow the IO dependency relations.
 * This is different from the schedule from Halide or TVM, in CINN, the Transform is decoupled from Schedule.
 */
std::unique_ptr<Schedule> CreateSchedule(const std::vector<Stage *> &stages);

/**
 * Gather the stages in the input tensors and their dependencies
 * @param xs The input tensors.
 * @param with_placeholder Whether to include placeholders(default false).
 * @returns The stages in topological order follow the connection to `xs`.
 */
std::vector<Stage *> GatherStagesInTensors(const std::vector<ir::Tensor> &xs, bool with_placeholder = false);

/**
 * PolyScheduler - Perform schedule on polyhedral model.
 * It takes a normal schedule as input, merge two stages automatically if they have the same domain.
 */
class PolyScheduler : public SchedulerBase {
 public:
  /**
   * Constructor.
   * @param schedule A normal isl schedule, such as '{ S[i,j] -> [i,j] }'
   *
   * The schedule input can be transformed, that's ok, such as
   *   '{ S[i,j] -> [i_outer, i_inner, j]: i_outer=floor(i/4) and i_inner=i%4 }'
   * that's OK.
   */
  PolyScheduler() = default;
  PolyScheduler(const std::vector<Stage *> &stages);

  /**
   * Mark this should schedule after another.
   *
   * @param b
   * @param level
   */
  PolyScheduler &After(const Stage &a, const Stage &b, int level);
  /**
   * Mark this should schedule before another.
   * @param b
   * @param level
   */
  PolyScheduler &Before(const Stage &a, const Stage &b, int level);

  /**
   * Build and create schedule.
   */
  std::map<std::string, isl::map> BuildSchedule() const;

  /**
   * Wrap the iterator names with time space fake names, it is used for isl AST to set iterator names.
   * @param names the original iterator names.
   * @return the iterator names with time space included.
   */
  std::vector<std::string> WrapIteratorNames(const std::vector<std::string> &names) const;

  int space_size() const { return space_size_; }

  const std::vector<std::string> &detailed_dimension_names() const { return detailed_dimension_names_; }
};

}  // namespace poly
}  // namespace cinn
