/**
 * Copyright 2019, The Jelly Bean World Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

#ifndef JBW_GIBBS_FIELD_H_
#define JBW_GIBBS_FIELD_H_

#include <stdio.h>
#include <Python.h>

#include <core/random.h>
#include <math/log.h>
#include "position.h"
#include "energy_functions.h"

#define GIBBS_SAMPLING 0
#define MH_SAMPLING 1

#define SAMPLING_METHOD MH_SAMPLING

namespace jbw {

using namespace core;

/**
 * Structure for optimizing gibbs_field sampling when the intensity and/or
 * interaction functions are stationary.
 */
template<typename ItemType>
struct gibbs_field_cache
{
	float* intensities;
	float** interactions;
	float* regenerations;
	unsigned int two_n, four_n;

	const ItemType* item_types;
	unsigned int item_type_count;

#if SAMPLING_METHOD == GIBBS_SAMPLING
	/* the list of patch positions to visit during each Gibbs iteration;
	   this will be shuffled at the beginning of each iteration */
	position* bottom_left_positions;
	position* top_left_positions;
	position* bottom_right_positions;
	position* top_right_positions;
#endif

	gibbs_field_cache(const ItemType* item_types, unsigned int item_type_count, unsigned int n) :
		two_n(2*n), four_n(4*n), item_types(item_types), item_type_count(item_type_count)
	{
		if (!init_helper(n)) exit(EXIT_FAILURE);
	}

	~gibbs_field_cache() { free_helper(); }

	inline float intensity(const position& pos, unsigned int item_type) {
		if (is_stationary(item_types[item_type].intensity_fn.fn))
			return intensities[item_type];
		else return item_types[item_type].intensity_fn.fn(pos, item_types[item_type].intensity_fn.args);
	}

	inline float regeneration(const position& pos, const uint64_t time, unsigned int item_type) {
		if (is_stationary(item_types[item_type].regeneration_fn.fn) && is_time_independent(item_types[item_type].regeneration_fn.fn)) {
			return regenerations[item_type];
		}
		else return item_types[item_type].regeneration_fn.fn(pos, time, item_types[item_type].regeneration_fn.args);
	}

	inline float interaction(
			const position& first_position, const position& second_position,
			unsigned int first_item_type, unsigned int second_item_type)
	{
		interaction_function interaction = item_types[first_item_type].interaction_fns[second_item_type].fn;
		if (is_constant(interaction) || !is_stationary(interaction)) {
			if (first_position == second_position) return 0.0f;
			return interaction(first_position, second_position, item_types[first_item_type].interaction_fns[second_item_type].args);
		} else {
			position diff = first_position - second_position + position(two_n, two_n);
#if !defined(NDEBUG)
			if (diff.x < 0 || diff.x >= four_n || diff.y < 0 || diff.y >= four_n) {
				fprintf(stderr, "gibbs_field_cache.interaction WARNING: The "
						"given two positions further than 4*n from each other.");
				return 0.0f;
			}
#endif
			return interactions[first_item_type*item_type_count + second_item_type][diff.x*four_n + diff.y];
		}
	}

	static inline void free(gibbs_field_cache& cache) { cache.free_helper(); }

private:
	inline bool init_helper(unsigned int n)
	{
#if SAMPLING_METHOD == GIBBS_SAMPLING
		bottom_left_positions = NULL;
		top_left_positions = NULL;
		bottom_right_positions = NULL;
		top_right_positions = NULL;
#endif
		intensities = (float*) malloc(sizeof(float) * item_type_count);
		if (intensities == NULL) {
			fprintf(stderr, "gibbs_field_cache.init_helper ERROR: Insufficient memory for intensities.\n");
			return false;
		}
		regenerations = (float*) malloc(sizeof(float) * item_type_count);
		if (regenerations == NULL) {
			fprintf(stderr, "gibbs_field_cache.init_helper ERROR: Insufficient memory for regenerations.\n");
			return false;
		}
		interactions = (float**) calloc(item_type_count * item_type_count, sizeof(float*));
		if (interactions == NULL) {
			fprintf(stderr, "gibbs_field_cache.init_helper ERROR: Insufficient memory for interactions.\n");
			core::free(intensities);
			return false;
		}
		for (unsigned int i = 0; i < item_type_count; i++) {
			if (is_stationary(item_types[i].intensity_fn.fn))
				intensities[i] = item_types[i].intensity_fn.fn(position(0, 0), item_types[i].intensity_fn.args);

			if (is_stationary(item_types[i].regeneration_fn.fn) && is_time_independent(item_types[i].regeneration_fn.fn)) {
				regenerations[i] = item_types[i].regeneration_fn.fn(position(0, 0), 0, item_types[i].regeneration_fn.args);
			}

			for (unsigned int j = 0; j < item_type_count; j++) {
				interaction_function interaction = item_types[i].interaction_fns[j].fn;
				if (!is_constant(interaction) && is_stationary(interaction)) {
					interactions[i*item_type_count + j] = (float*) malloc(sizeof(float) * four_n * four_n);
					if (interactions[i*item_type_count + j] == NULL) {
						fprintf(stderr, "gibbs_field_cache.init_helper ERROR: Insufficient memory for interactions.\n");
						free_helper(); return false;
					}

					for (unsigned int x = 0; x < four_n; x++) {
						for (unsigned int y = 0; y < four_n; y++) {
							float value;
							if (x == two_n && y == two_n)
								value = 0.0f;
							else value = interaction(position(two_n, two_n), position(x, y), item_types[i].interaction_fns[j].args);
							interactions[i*item_type_count + j][x*four_n + y] = value;
						}
					}
				} else {
					interactions[i*item_type_count + j] = NULL;
				}
			}
		}

#if SAMPLING_METHOD == GIBBS_SAMPLING
		unsigned int half_n = n / 2;
		bottom_left_positions = (position*) malloc(sizeof(position) * half_n * half_n);
		top_left_positions = (position*) malloc(sizeof(position) * half_n * half_n);
		bottom_right_positions = (position*) malloc(sizeof(position) * half_n * half_n);
		top_right_positions = (position*) malloc(sizeof(position) * half_n * half_n);
		if (bottom_left_positions == NULL || top_left_positions == NULL || bottom_right_positions == NULL || top_right_positions == NULL) {
			fprintf(stderr, "gibbs_field_cache.init_helper ERROR: Insufficient memory for position_list.\n");
			free_helper(); return false;
		}
		unsigned int i = 0;
		for (unsigned int x = 0; x < half_n; x++)
			for (unsigned int y = 0; y < half_n; y++)
				bottom_left_positions[i++] = position(x, y);
		i = 0;
		for (unsigned int x = 0; x < half_n; x++)
			for (unsigned int y = half_n; y < n; y++)
				top_left_positions[i++] = position(x, y);
		i = 0;
		for (unsigned int x = half_n; x < n; x++)
			for (unsigned int y = 0; y < half_n; y++)
				bottom_right_positions[i++] = position(x, y);
		i = 0;
		for (unsigned int x = half_n; x < n; x++)
			for (unsigned int y = half_n; y < n; y++)
				top_right_positions[i++] = position(x, y);
#endif
		return true;
	}

	inline void free_helper() {
		core::free(intensities);
		for (unsigned int i = 0; i < item_type_count * item_type_count; i++)
			if (interactions[i] != NULL) core::free(interactions[i]);
		core::free(interactions);
		core::free(regenerations);
#if SAMPLING_METHOD == GIBBS_SAMPLING
		if (bottom_left_positions != NULL) core::free(bottom_left_positions);
		if (top_left_positions != NULL) core::free(top_left_positions);
		if (bottom_right_positions != NULL) core::free(bottom_right_positions);
		if (top_right_positions != NULL) core::free(top_right_positions);
#endif
	}

	template<typename A>
	friend bool init(gibbs_field_cache<A>&, const A*, unsigned int, unsigned int);
};

template<typename ItemType>
bool init(gibbs_field_cache<ItemType>& cache,
		const ItemType* item_types, unsigned int item_type_count, unsigned int n)
{
	cache.two_n = 2*n;
	cache.four_n = 4*n;
	cache.item_types = item_types;
	cache.item_type_count = item_type_count;
	return cache.init_helper(n);
}

template<typename T, typename RNGType>
void shuffle(T* array, unsigned int length, RNGType& rng) {
#if !defined(NDEBUG)
	if (length == 0) {
		fprintf(stderr, "shuffle WARNING: Length is zero.\n");
		return;
	}
#endif
	for (unsigned int i = length - 1; i > 0; i--) {
		unsigned int next = rng() % (i + 1);
		if (next != i)
			core::swap(array[next], array[i]);
	}
}

template<typename PatchType>
struct patch_neighborhood {
	PatchType* bottom_left_neighborhood[4];
	PatchType* top_left_neighborhood[4];
	PatchType* bottom_right_neighborhood[4];
	PatchType* top_right_neighborhood[4];
	uint_fast8_t bottom_left_neighbor_count;
	uint_fast8_t top_left_neighbor_count;
	uint_fast8_t bottom_right_neighbor_count;
	uint_fast8_t top_right_neighbor_count;
};

template<typename Map>
class gibbs_field
{
	typedef typename Map::patch_type patch_type;
	typedef typename Map::item_type item_type;

	gibbs_field_cache<item_type>& cache;
	const position* patch_positions;
	const patch_neighborhood<patch_type>* neighborhoods;
	const unsigned int patch_count;

	const unsigned int n;

#if SAMPLING_METHOD == MH_SAMPLING
	float LOG_ITEM_TYPE_COUNT;
	float LOG_N_SQUARED;
#endif

public:
	/* NOTE: `patch_positions` and `neighborhoods` is used directly, and not copied, so the caller maintains ownership */
	gibbs_field(
			gibbs_field_cache<item_type>& cache,
			const position* patch_positions,
			const patch_neighborhood<patch_type>* neighborhoods,
			unsigned int patch_count, unsigned int n) :
		cache(cache), patch_positions(patch_positions),
		neighborhoods(neighborhoods), patch_count(patch_count), n(n)
	{
#if SAMPLING_METHOD == MH_SAMPLING
		LOG_ITEM_TYPE_COUNT = (float) log(cache.item_type_count);
		LOG_N_SQUARED = (float) (2 * log(n));
#endif
	}

	~gibbs_field() { }

	template<typename RNGType>
	void sample(RNGType& rng) {
#if SAMPLING_METHOD == MH_SAMPLING
		log_cache<float>& logarithm = log_cache<float>::instance();
#endif
		for (unsigned int i = 0; i < patch_count; i++) {
			const position patch_position_offset = patch_positions[i] * n;
			const patch_neighborhood<patch_type>& neighborhood = neighborhoods[i];

#if SAMPLING_METHOD == GIBBS_SAMPLING
			unsigned int half_n_squared = (n / 2) * (n / 2);
			shuffle(cache.bottom_left_positions, half_n_squared, rng);
			shuffle(cache.top_left_positions, half_n_squared, rng);
			shuffle(cache.bottom_right_positions, half_n_squared, rng);
			shuffle(cache.top_right_positions, half_n_squared, rng);

			for (unsigned int j = 0; j < half_n_squared; j++)
				gibbs_sample_cell(rng, neighborhood.bottom_left_neighborhood, neighborhood.bottom_left_neighbor_count, patch_position_offset + cache.bottom_left_positions[j]);
			for (unsigned int j = 0; j < half_n_squared; j++)
				gibbs_sample_cell(rng, neighborhood.top_right_neighborhood, neighborhood.top_right_neighbor_count,  patch_position_offset + cache.top_right_positions[j]);
			for (unsigned int j = 0; j < half_n_squared; j++)
				gibbs_sample_cell(rng, neighborhood.top_left_neighborhood, neighborhood.top_left_neighbor_count, patch_position_offset + cache.top_left_positions[j]);
			for (unsigned int j = 0; j < half_n_squared; j++)
				gibbs_sample_cell(rng, neighborhood.bottom_right_neighborhood, neighborhood.bottom_right_neighbor_count, patch_position_offset + cache.bottom_right_positions[j]);

#elif SAMPLING_METHOD == MH_SAMPLING

			patch_type& current = *neighborhood.top_left_neighborhood[0];
			if (rng() % 2 == 0) {
				/* propose creating a new item */
				const unsigned int item_type = rng() % cache.item_type_count;
				position new_position = patch_position_offset + position(rng() % n, rng() % n);

				patch_type* const* new_neighborhood;
				uint_fast8_t new_neighborhood_size;
				if (new_position.x - patch_position_offset.x < n / 2) {
					if (new_position.y - patch_position_offset.y < n / 2) {
						new_neighborhood = neighborhood.bottom_left_neighborhood;
						new_neighborhood_size = neighborhood.bottom_left_neighbor_count;
					} else {
						new_neighborhood = neighborhood.top_left_neighborhood;
						new_neighborhood_size = neighborhood.top_left_neighbor_count;
					}
				} else {
					if (new_position.y - patch_position_offset.y < n / 2) {
						new_neighborhood = neighborhood.bottom_right_neighborhood;
						new_neighborhood_size = neighborhood.bottom_right_neighbor_count;
					} else {
						new_neighborhood = neighborhood.top_right_neighborhood;
						new_neighborhood_size = neighborhood.top_right_neighbor_count;
					}
				}

				float log_acceptance_probability = 0.0f;
				bool new_position_occupied = false;
				for (uint_fast8_t j = 0; j < new_neighborhood_size; j++) {
					const auto& items = new_neighborhood[j]->items;
					for (unsigned int m = 0; m < items.length; m++) {
						if (items[m].location == new_position) {
							/* an item already exists at this proposed location */
							new_position_occupied = true; break;
						}
						log_acceptance_probability += cache.interaction(new_position, items[m].location, item_type, items[m].item_type);
						log_acceptance_probability += cache.interaction(items[m].location, new_position, items[m].item_type, item_type);
					}
					if (new_position_occupied) break;
				}
				if (!new_position_occupied) {
					log_acceptance_probability += cache.intensity(new_position, item_type);

					/* add log probability of inverse proposal */
					logarithm.ensure_size((unsigned int) current.items.length + 2);
					log_acceptance_probability += (float) -logarithm.get((unsigned int) current.items.length + 1);

					/* subtract log probability of forward proposal */
					log_acceptance_probability -= -LOG_ITEM_TYPE_COUNT - LOG_N_SQUARED;

					/* accept or reject the proposal depending on the computed probability */
					float random = (float) rng() / rng.max();
					if (log(random) < log_acceptance_probability) {
						/* accept the proposal */
						current.items.add({ item_type, new_position, 0, 0 });
					}
				}

			} else if (current.items.length > 0) {
				/* propose deleting an item */
				unsigned int item_index = rng() % current.items.length;
				const unsigned int old_item_type = current.items[item_index].item_type;
				const position old_position = current.items[item_index].location;

				patch_type* const* old_neighborhood;
				uint_fast8_t old_neighborhood_size;
				if (old_position.x - patch_position_offset.x < n / 2) {
					if (old_position.y - patch_position_offset.y < n / 2) {
						old_neighborhood = neighborhood.bottom_left_neighborhood;
						old_neighborhood_size = neighborhood.bottom_left_neighbor_count;
					} else {
						old_neighborhood = neighborhood.top_left_neighborhood;
						old_neighborhood_size = neighborhood.top_left_neighbor_count;
					}
				} else {
					if (old_position.y - patch_position_offset.y < n / 2) {
						old_neighborhood = neighborhood.bottom_right_neighborhood;
						old_neighborhood_size = neighborhood.bottom_right_neighbor_count;
					} else {
						old_neighborhood = neighborhood.top_right_neighborhood;
						old_neighborhood_size = neighborhood.top_right_neighbor_count;
					}
				}

				float log_acceptance_probability = 0.0f;
				for (uint_fast8_t j = 0; j < old_neighborhood_size; j++) {
					const auto& items = old_neighborhood[j]->items;
					for (unsigned int m = 0; m < items.length; m++) {
						log_acceptance_probability -= cache.interaction(old_position, items[m].location, old_item_type, items[m].item_type);
						log_acceptance_probability -= cache.interaction(items[m].location, old_position, items[m].item_type, old_item_type);
					}
				}
				log_acceptance_probability -= cache.intensity(old_position, old_item_type);

				/* add log probability of inverse proposal */
				log_acceptance_probability += -LOG_ITEM_TYPE_COUNT - LOG_N_SQUARED;

				/* subtract log probability of forward proposal */
				logarithm.ensure_size((unsigned int) current.items.length + 1);
				log_acceptance_probability -= (float) -logarithm.get((unsigned int) current.items.length);

				/* accept or reject the proposal depending on the computed probability */
				float random = (float) rng() / rng.max();
				if (log(random) < log_acceptance_probability) {
					/* accept the proposal */
					current.items.remove(item_index);
				}
			}
#endif /* SAMPLING_METHOD == MH_SAMPLING */
		}
	}

	template<typename RNGType>
	void resample(RNGType& rng, uint64_t current_time) {
#if SAMPLING_METHOD == MH_SAMPLING
		log_cache<float>& logarithm = log_cache<float>::instance();
#endif
		/* Retrieving regeneration rate */

		for (unsigned int i = 0; i < patch_count; i++) {
			const position patch_position_offset = patch_positions[i] * n;
			const patch_neighborhood<patch_type>& neighborhood = neighborhoods[i];

			patch_type& current = *neighborhood.top_left_neighborhood[0];

			/* New intensity function */
			float real_intensity = log(current.items.length) - LOG_N_SQUARED;
			float r = 0; //regeneration rate with the cache
			float new_intensity = log(1+r) + real_intensity;

			/* Backup */
			// sample(rng);
		}
	}

private:
	/* NOTE: we assume `neighborhood[0]` is the patch we're sampling */
	template<typename RNGType>
	inline void gibbs_sample_cell(RNGType& rng,
			patch_type* const neighborhood[4],
			unsigned int neighbor_count,
			const position& world_position)
	{
		/* compute the old item type and index */
		patch_type& current_patch = *neighborhood[0];
		unsigned int old_item_index = 0, old_item_type = cache.item_type_count;
		for (unsigned int m = 0; m < current_patch.items.length; m++) {
			if (current_patch.items[m].location == world_position) {
				old_item_type = current_patch.items[m].item_type;
				old_item_index = m; break;
			}
		}

		float* log_probabilities = (float*) alloca(sizeof(float) * (cache.item_type_count + 1));
		for (unsigned int i = 0; i < cache.item_type_count; i++)
			log_probabilities[i] = cache.intensity(world_position, i);
		for (unsigned int j = 0; j < neighbor_count; j++) {
			const auto& items = neighborhood[j]->items;
			for (unsigned int m = 0; m < items.length; m++) {
				for (unsigned int i = 0; i < cache.item_type_count; i++) {
					/* compute the energy contribution of this cell when the item type is `i` */
					log_probabilities[i] += cache.interaction(world_position, items[m].location, i, items[m].item_type);
					log_probabilities[i] += cache.interaction(items[m].location, world_position, items[m].item_type, i);
				}
			}
		}

		log_probabilities[cache.item_type_count] = 0.0;
		normalize_exp(log_probabilities, cache.item_type_count + 1);
		float random = (float) rng() / rng.max();
		unsigned int sampled_item_type = select_categorical(
				log_probabilities, random, cache.item_type_count + 1);

		if (old_item_type == sampled_item_type) {
			/* the Gibbs step didn't change anything */
			return;
		} if (old_item_type < cache.item_type_count) {
			/* remove the old item position */
			current_patch.items.remove(old_item_index);
		} if (sampled_item_type < cache.item_type_count) {
			/* add the new item position */
			current_patch.items.add({sampled_item_type, world_position, 0, 0});
		}
	}
};

} /* namespace jbw */

#endif /* JBW_GIBBS_FIELD_H_ */
