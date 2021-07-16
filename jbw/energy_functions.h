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

#ifndef ENERGY_FUNCTIONS_H_
#define ENERGY_FUNCTIONS_H_

#include <math.h>
#include "position.h"

namespace jbw {

typedef float (*intensity_function)(const position, const float*);
typedef float (*interaction_function)(const position, const position, const float*);
typedef float (*regeneration_function)(const position, const uint64_t, const float*);


typedef uint64_t intensity_fns_type;
enum class intensity_fns : intensity_fns_type {
	ZERO = 0, CONSTANT, RADIAL_HASH
};

typedef uint64_t interaction_fns_type;
enum class interaction_fns : interaction_fns_type {
	ZERO = 0, PIECEWISE_BOX, CROSS, CROSS_HASH, MOORE, GAUSSIAN, FOUR
};

typedef uint64_t regeneration_fns_type;
enum class regeneration_fns : regeneration_fns_type {
	ZERO = 0, CONSTANT, CUSTOM
};

float zero_intensity_fn(const position pos, const float* args) {
	return 0.0;
}

float constant_intensity_fn(const position pos, const float* args) {
	return args[0];
}

inline uint32_t murmurhash32_mix32(uint32_t x) {
    x ^= x >> 16;
    x *= (uint32_t) 0x45d9f3b;
    x ^= x >> 16;
    x *= (uint32_t) 0x45d9f3b;
    x ^= x >> 16;
    return x;
}

float hash_function(uint32_t x, uint32_t shift, uint32_t scale) {
	return (float) murmurhash32_mix32((x + shift) / scale) / (float) UINT_MAX;
}

float radial_hash_intensity_fn(const position pos, const float* args) {
	uint32_t shift = (uint32_t) args[0];
	uint32_t scale = (uint32_t) args[1];

	uint32_t s = (uint32_t) sqrt(pos.squared_length()) + shift;
	float x = hash_function(s, shift, scale);
	float x_next = hash_function(s + scale, shift, scale);

	float t_x = (float) (s % scale) / scale;
	if (t_x < 0) t_x += 1;
	return args[2] - (x * (1 - t_x) + x_next * t_x) * args[3];
}

intensity_function get_intensity_fn(intensity_fns type, const float* args, unsigned int num_args)
{
	switch (type) {
	case intensity_fns::ZERO:
		if (num_args != 0) {
			fprintf(stderr, "get_intensity_fn ERROR: A zero intensity function requires zero arguments.");
			return NULL;
		}
		return zero_intensity_fn;
	case intensity_fns::CONSTANT:
		if (num_args == 0) {
			fprintf(stderr, "get_intensity_fn ERROR: A constant intensity function requires an argument.");
			return NULL;
		}
		return constant_intensity_fn;
	case intensity_fns::RADIAL_HASH:
		if (num_args != 4) {
			fprintf(stderr, "get_intensity_fn ERROR: A radial hash intensity function requires 4 arguments.");
			return NULL;
		}
		return radial_hash_intensity_fn;
	}
	fprintf(stderr, "get_intensity_fn ERROR: Unknown intensity function type.");
	return NULL;
}

intensity_fns get_intensity_fn(intensity_function function) {
	if (function == zero_intensity_fn) {
		return intensity_fns::ZERO;
	} else if (function == constant_intensity_fn) {
		return intensity_fns::CONSTANT;
	} else if (function == radial_hash_intensity_fn) {
		return intensity_fns::RADIAL_HASH;
	} else {
		fprintf(stderr, "get_intensity_fn ERROR: Unknown intensity_function.");
		exit(EXIT_FAILURE);
	}
}

float zero_interaction_fn(const position pos1, const position pos2, const float* args) {
	return 0.0;
}

float piecewise_box_interaction_fn(const position pos1, const position pos2, const float* args)
{
	float first_cutoff = args[0];
	float second_cutoff = args[1];
	float first_value = args[2];
	float second_value = args[3];

	uint64_t squared_length = (pos1 - pos2).squared_length();
	if (squared_length < first_cutoff)
		return first_value;
	else if (squared_length < second_cutoff)
		return second_value;
	else return 0.0f;
}

float gaussian_interaction_fn(const position pos1, const position pos2, const float* args){
	const position diff = pos1 - pos2;
	float dist = exp(-diff.x*diff.x/(2*args[0]*args[0]) - diff.y*diff.y/(2*args[0]*args[0]));
	return args[1]*dist;
}

float cross_interaction_fn(const position pos1, const position pos2, const float* args)
{
	const position diff = pos1 - pos2;
	uint64_t dist = max(abs(diff.x), abs(diff.y));
	if (dist <= args[0]) {
		if (diff.x == 0 || diff.y == 0)
			return args[2];
		else return args[4];
	} else if (dist <= args[1]) {
		if (diff.x == 0 || diff.y == 0)
			return args[3];
		else return args[5];
	} else {
		return 0.0f;
	}
}

float cross_hash_interaction_fn(const position pos1, const position pos2, const float* args)
{
	uint32_t scale = (uint32_t) args[0];
	float x = hash_function((uint32_t) pos1.x, 0, scale);
	float x_next = hash_function((uint32_t) pos1.x + scale, 0, scale);
	float t_x = (float) ((uint32_t) pos1.x % scale) / scale;
	if (t_x < 0) t_x += 1;

	float d = args[2]*(x * (1 - t_x) + x_next * t_x) + args[1];
	float D = d + args[3];

	const position diff = pos1 - pos2;
	uint64_t dist = max(abs(diff.x), abs(diff.y));
	if (dist <= d) {
		if (diff.x == 0 || diff.y == 0)
			return args[4];
		else return args[6];
	} else if (dist <= D) {
		if (diff.x == 0 || diff.y == 0)
			return args[5];
		else return args[7];
	} else {
		return 0.0f;
	}
}

float moore_interaction_fn(const position pos1, const position pos2, const float* args) {
	const position diff = pos1 - pos2;
	if (abs(diff.x) < 2 && abs(diff.y) < 2)
		return 1.0;
	else return -200.0f;
}

float four_interaction_fn(const position pos1, const position pos2, const float* args) {
	const position diff = pos1 - pos2;
	if (abs(diff.x) < 1 && abs(diff.y) < 2)
		return 1.0;
	else {
		if (abs(diff.y) < 1 && abs(diff.x) < 2)
			return 1.0;
		else return -200.0f;
	}
}

interaction_function get_interaction_fn(interaction_fns type, const float* args, unsigned int num_args)
{
	switch (type) {
	case interaction_fns::ZERO:
		if (num_args != 0) {
			fprintf(stderr, "get_interaction_fn ERROR: A zero interaction function requires zero arguments.");
			return NULL;
		}
		return zero_interaction_fn;
	case interaction_fns::PIECEWISE_BOX:
		if (num_args != 4) {
			fprintf(stderr, "get_interaction_fn ERROR: A piecewise-box integration function requires 4 arguments.");
			return NULL;
		}
		return piecewise_box_interaction_fn;
	case interaction_fns::CROSS:
		if (num_args != 6) {
			fprintf(stderr, "get_interaction_fn ERROR: A cross integration function requires 6 arguments.");
			return NULL;
		}
		return cross_interaction_fn;
	case interaction_fns::CROSS_HASH:
		if (num_args != 8) {
			fprintf(stderr, "get_interaction_fn ERROR: A cross-hash integration function requires 6 arguments.");
			return NULL;
		}
		return cross_hash_interaction_fn;
	case interaction_fns::MOORE:
		if (num_args != 0) {
			fprintf(stderr, "get_interaction_fn ERROR: A moore interaction function requires zero arguments.");
			return NULL;
		}
		return moore_interaction_fn;
	case interaction_fns::FOUR:
		if (num_args != 0) {
			fprintf(stderr, "get_interaction_fn ERROR: A four interaction function requires zero arguments.");
			return NULL;
		}
		return four_interaction_fn;	
	case interaction_fns::GAUSSIAN:
		if (num_args != 2) {
			fprintf(stderr, "get_interaction_fn ERROR: A gaussian interaction function requires 2 arguments.");
			return NULL;
		}
		return gaussian_interaction_fn;	
	}
	fprintf(stderr, "get_interaction_fn ERROR: Unknown interaction function type.");
	return NULL;
}

interaction_fns get_interaction_fn(interaction_function function) {
	if (function == zero_interaction_fn) {
		return interaction_fns::ZERO;
	} else if (function == piecewise_box_interaction_fn) {
		return interaction_fns::PIECEWISE_BOX;
	} else if (function == cross_interaction_fn) {
		return interaction_fns::CROSS;
	} else if (function == cross_hash_interaction_fn) {
		return interaction_fns::CROSS_HASH;
	} else if (function == moore_interaction_fn) {
		return interaction_fns::MOORE;
	} else if (function == four_interaction_fn) {
		return interaction_fns::FOUR;
	} else if (function == gaussian_interaction_fn) {
		return interaction_fns::GAUSSIAN;
	} else {
		fprintf(stderr, "get_interaction_fn ERROR: Unknown interaction_function.");
		exit(EXIT_FAILURE);
	}
}

float zero_regeneration_fn(const position pos, const uint64_t time, const float* args) {
	return 0.0;
}

float constant_regeneration_fn(const position pos, const uint64_t time, const float* args) {
	return args[0];
}

float custom_regeneration_fn(const position pos, const uint64_t time, const float* args) {
	return args[time];
}

regeneration_function get_regeneration_fn(regeneration_fns type, const float* args, unsigned int num_args)
{
	switch (type) {
	case regeneration_fns::ZERO:
		if (num_args != 0) {
			fprintf(stderr, "get_regeneration_fn ERROR: A zero regeneration function requires zero arguments.");
			return NULL;
		}
		return zero_regeneration_fn;
	case regeneration_fns::CONSTANT:
		if (num_args == 0) {
			fprintf(stderr, "get_regeneration_fn ERROR: A constant regeneration function requires an argument.");
			return NULL;
		}
		return constant_regeneration_fn;
	case regeneration_fns::CUSTOM:
		if (num_args == 0) {
			fprintf(stderr, "get_regeneration_fn ERROR: A custom regeneration function requires at least an argument.");
			return NULL;
		}
		return custom_regeneration_fn;
	}
	fprintf(stderr, "get_regeneration_fn ERROR: Unknown intensity function type.");
	return NULL;
}

regeneration_fns get_regeneration_fn(regeneration_function function) {
	if (function == zero_regeneration_fn) {
		return regeneration_fns::ZERO;
	} else if (function == constant_regeneration_fn) {
		return regeneration_fns::CONSTANT;
	} else if (function == custom_regeneration_fn) {
		return regeneration_fns::CUSTOM;
	} else {
		fprintf(stderr, "get_regeneration_fn ERROR: Unknown regeneration_function.");
		exit(EXIT_FAILURE);
	}
}

template<typename Stream>
inline bool read(intensity_function& function, Stream& in) {
	intensity_fns_type c;
	if (!read(c, in)) return false;
	switch ((intensity_fns) c) {
	case intensity_fns::ZERO:        function = zero_intensity_fn; return true;
	case intensity_fns::CONSTANT:    function = constant_intensity_fn; return true;
	case intensity_fns::RADIAL_HASH: function = radial_hash_intensity_fn; return true;
	}
	fprintf(stderr, "read ERROR: Unrecognized intensity function.\n");
	return false;
}

template<typename Stream>
inline bool write(const intensity_function& function, Stream& out) {
	if (function == zero_intensity_fn) {
		return write((intensity_fns_type) intensity_fns::ZERO, out);
	} else if (function == constant_intensity_fn) {
		return write((intensity_fns_type) intensity_fns::CONSTANT, out);
	} else if (function == radial_hash_intensity_fn) {
		return write((intensity_fns_type) intensity_fns::RADIAL_HASH, out);
	} else {
		fprintf(stderr, "write ERROR: Unrecognized intensity function.\n");
		return false;
	}
}

template<typename Stream>
inline bool read(interaction_function& function, Stream& in) {
	interaction_fns_type c;
	if (!read(c, in)) return false;
	switch ((interaction_fns) c) {
	case interaction_fns::ZERO:          function = zero_interaction_fn; return true;
	case interaction_fns::PIECEWISE_BOX: function = piecewise_box_interaction_fn; return true;
	case interaction_fns::CROSS:         function = cross_interaction_fn; return true;
	case interaction_fns::CROSS_HASH:    function = cross_hash_interaction_fn; return true;
	case interaction_fns::MOORE:       	 function = moore_interaction_fn; return true;
	case interaction_fns::FOUR:       	 function = four_interaction_fn; return true;
	case interaction_fns::GAUSSIAN:    	 function = gaussian_interaction_fn; return true;
	}
	fprintf(stderr, "read ERROR: Unrecognized interaction function.\n");
	return false;
}

template<typename Stream>
inline bool write(const interaction_function& function, Stream& out) {
	if (function == zero_interaction_fn) {
		return write((interaction_fns_type) interaction_fns::ZERO, out);
	} else if (function == piecewise_box_interaction_fn) {
		return write((interaction_fns_type) interaction_fns::PIECEWISE_BOX, out);
	} else if (function == cross_interaction_fn) {
		return write((interaction_fns_type) interaction_fns::CROSS, out);
	} else if (function == cross_hash_interaction_fn) {
		return write((interaction_fns_type) interaction_fns::CROSS_HASH, out);
	} else if (function == moore_interaction_fn) {
		return write((interaction_fns_type) interaction_fns::MOORE, out);
	} else if (function == four_interaction_fn) {
		return write((interaction_fns_type) interaction_fns::FOUR, out);
	} else if (function == gaussian_interaction_fn) {
		return write((interaction_fns_type) interaction_fns::GAUSSIAN, out);
	} else {
		fprintf(stderr, "write ERROR: Unrecognized interaction function.\n");
		return false;
	}
}

template<typename Stream>
inline bool read(regeneration_function& function, Stream& in) {
	regeneration_fns_type c;
	if (!read(c, in)) return false;
	switch ((regeneration_fns) c) {
	case regeneration_fns::ZERO:          function = zero_regeneration_fn; return true;
	case regeneration_fns::CONSTANT: function = constant_regeneration_fn; return true;
	case regeneration_fns::CUSTOM:         function = custom_regeneration_fn; return true;
	}
	fprintf(stderr, "read ERROR: Unrecognized regeneration function.\n");
	return false;
}

template<typename Stream>
inline bool write(const regeneration_function& function, Stream& out) {
	if (function == zero_regeneration_fn) {
		return write((regeneration_fns_type) regeneration_fns::ZERO, out);
	} else if (function == constant_regeneration_fn) {
		return write((regeneration_fns_type) regeneration_fns::CONSTANT, out);
	} else if (function == custom_regeneration_fn) {
		return write((regeneration_fns_type) regeneration_fns::CUSTOM, out);
	} else {
		fprintf(stderr, "write ERROR: Unrecognized regeneration function.\n");
		return false;
	}
}

inline bool is_constant(const interaction_function function) {
	return function == zero_interaction_fn;
}

inline bool is_constant(const regeneration_function function) {
	return function == zero_regeneration_fn;
}

/* NOTE: stationary intensity functions are also constant */
inline bool is_stationary(const intensity_function function) {
	return (function == zero_intensity_fn
		 || function == constant_intensity_fn);
}

inline bool is_stationary(const interaction_function function) {
	return (function == zero_interaction_fn
		 || function == piecewise_box_interaction_fn
		 || function == cross_interaction_fn
		 || function == moore_interaction_fn);
}

/* NOTE: stationary regeneration functions are also constant */
inline bool is_stationary(const regeneration_function function) {
	return (function == zero_regeneration_fn
		 || function == constant_regeneration_fn);
}

inline bool is_time_independent(const regeneration_function function) {
	return (function == zero_regeneration_fn
		 || function == constant_regeneration_fn);
}

inline bool is_custom(const regeneration_function function) {
	return (function == custom_regeneration_fn);
}

float precipitations(const position pos, const uint64_t time) {
	// Between 0 and 100
	// float period = 600;
	// return 50 + 45*sin(2*3.14*time/period);
	// return 20;
	if (time < 1000) return 5;
	else {
		if (time < 2500) return 80;
		else return 5;
	}
	// return 80;
}

} /* namespace jbw */

#endif /* ENERGY_FUNCTIONS_H_ */
