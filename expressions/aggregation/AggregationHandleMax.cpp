/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 **/

#include "expressions/aggregation/AggregationHandleMax.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "catalog/CatalogTypedefs.hpp"
#include "expressions/aggregation/AggregationID.hpp"
#include "storage/ValueAccessorMultiplexer.hpp"
#include "types/Type.hpp"
#include "types/TypedValue.hpp"
#include "types/operations/comparisons/Comparison.hpp"
#include "types/operations/comparisons/ComparisonFactory.hpp"
#include "types/operations/comparisons/ComparisonID.hpp"

#include "glog/logging.h"

namespace quickstep {

class ColumnVector;

AggregationHandleMax::AggregationHandleMax(const Type &type)
    : AggregationConcreteHandle(AggregationID::kMax),
      type_(type) {
  fast_comparator_.reset(
      ComparisonFactory::GetComparison(ComparisonID::kGreater)
          .makeUncheckedComparatorForTypes(type, type.getNonNullableVersion()));
}

AggregationState* AggregationHandleMax::accumulateValueAccessor(
    const std::vector<MultiSourceAttributeId> &argument_ids,
    const ValueAccessorMultiplexer &accessor_mux) const {
  DCHECK_EQ(1u, argument_ids.size())
      << "Got wrong number of attributes for MAX: " << argument_ids.size();

  const ValueAccessorSource argument_source = argument_ids.front().source;
  const attribute_id argument_id = argument_ids.front().attr_id;

  DCHECK(argument_source != ValueAccessorSource::kInvalid);
  DCHECK_NE(argument_id, kInvalidAttributeID);

  return new AggregationStateMax(fast_comparator_->accumulateValueAccessor(
      type_.getNullableVersion().makeNullValue(),
      accessor_mux.getValueAccessorBySource(argument_source),
      argument_id));
}

void AggregationHandleMax::mergeStates(const AggregationState &source,
                                       AggregationState *destination) const {
  const AggregationStateMax &max_source =
      static_cast<const AggregationStateMax &>(source);
  AggregationStateMax *max_destination =
      static_cast<AggregationStateMax *>(destination);

  if (!max_source.max_.isNull()) {
    compareAndUpdate(max_destination, max_source.max_);
  }
}

void AggregationHandleMax::mergeStates(const std::uint8_t *source,
                                       std::uint8_t *destination) const {
  const TypedValue *src_max_ptr = reinterpret_cast<const TypedValue *>(source);
  TypedValue *dst_max_ptr = reinterpret_cast<TypedValue *>(destination);
  if (!(src_max_ptr->isNull())) {
    compareAndUpdate(dst_max_ptr, *src_max_ptr);
  }
}

ColumnVector* AggregationHandleMax::finalizeHashTable(
    const AggregationStateHashTableBase &hash_table,
    const std::size_t index,
    std::vector<std::vector<TypedValue>> *group_by_keys) const {
  return finalizeHashTableHelper<AggregationHandleMax>(
      type_, hash_table, index, group_by_keys);
}

AggregationState* AggregationHandleMax::aggregateOnDistinctifyHashTableForSingle(
    const AggregationStateHashTableBase &distinctify_hash_table) const {
  return aggregateOnDistinctifyHashTableForSingleUnaryHelper<
      AggregationHandleMax, AggregationStateMax>(
          distinctify_hash_table);
}

void AggregationHandleMax::aggregateOnDistinctifyHashTableForGroupBy(
    const AggregationStateHashTableBase &distinctify_hash_table,
    const std::size_t index,
    AggregationStateHashTableBase *aggregation_hash_table) const {
  aggregateOnDistinctifyHashTableForGroupByUnaryHelper<AggregationHandleMax>(
      distinctify_hash_table, index, aggregation_hash_table);
}

}  // namespace quickstep
