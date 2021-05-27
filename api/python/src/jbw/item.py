# Copyright 2019, The Jelly Bean World Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

from __future__ import absolute_import, division, print_function

from enum import Enum

__all__ = ['Item', 'IntensityFunction', 'InteractionFunction', 'RegenerationFunction']

class RegenerationFunction(Enum):

  ZERO = 0
  """A function that always outputs zero: f_i(x, t) = 0."""

  CONSTANT = 1
  """A function that outputs a constant: f_i(x, t) = c. The arguments for this
  function should be a list of size 1 containing the constant: [c]."""

  CRENEL = 3
  """This function represents the climat dynamics described in the paper  
  A synthesis of the theories and concepts of early human evolution, Mark A. Maslin,
  Susanne Shultz and Martin H. Trauth Published:05 March 2015
  https://doi.org/10.1098/rstb.2014.0064 
  
  It's a temporal non-stationary function."""


class Item(object):
  """Represents an item in the world (e.g., jelly beans)."""

  def __init__(self, name, scent, color, required_item_counts, required_item_costs,
               blocks_movement, visual_occlusion, intensity_fn, intensity_fn_args, 
               interaction_fns, regeneration_fn = RegenerationFunction.ZERO, 
               regeneration_fn_args = [], lifetime = 0):
    """Creates a new item.
  
    Arguments:
      name:                 Name, represented as a string.
      scent:                Scent, represented as a list of floats.
      color:                Color, represented as a list of floats.
      required_item_counts: A list whose value at index `i` indicates the
                            minimum number of items of type `i` that need to be
                            collected by the agent in order to automatically
                            collect items of this type.
      required_item_costs:  A list whose value at index `i` indicates the
                            number of items of type `i` that are removed from
                            the agent's inventory whenever the agent collects
                            items of this type.
      blocks_movement:      Whether this item blocks movement of agents.
      visual_occlusion:     The amount by which this item occludes vision.
      intensity_fn:         The IntensityFunction used by the Gibbs sampler for
                            generating items of this type in the map.
      intensity_fn_args:    A list of float arguments to intensity_fn.
      interaction_fns:      A list of n lists, where n is the number of item
                            types. For each sublist interaction_fns[i], the
                            first element contains the InteractionFunction
                            between items of this type and items of type i, and
                            the remaining elements of the sublist contain the
                            parameters to this interaction function.
      regeneration_fn:      The RegenerationFunction used by the Gibbs sampler
                            for updating the patches in the map.
      regeneration_fn_args: A list of float arguments to regeneration_fn
      lifetime:             The number of timesteps after which the item is destroyed.
                            0 means an infinite lifetime.
    """
    self.name = name
    self.scent = scent
    self.color = color
    self.required_item_counts = required_item_counts
    self.required_item_costs = required_item_costs
    self.blocks_movement = blocks_movement
    self.visual_occlusion = visual_occlusion
    self.intensity_fn = intensity_fn.value
    self.intensity_fn_args = intensity_fn_args
    self.regeneration_fn = regeneration_fn.value
    self.regeneration_fn_args = regeneration_fn_args
    self.interaction_fns = interaction_fns
    self.lifetime = lifetime
    assert all([len(l) > 0 and type(l[0]) == InteractionFunction for l in interaction_fns]), 'Each sublist in `interaction_fns` must contain an InteractionFunction instance as the first element.'
    for l in interaction_fns:
      l[0] = l[0].value


class IntensityFunction(Enum):
  """Item intensity function used in the Gibbs sampler for map generation.
  See `jbw/energy_functions.h` for implementations of these functions."""

  ZERO = 0
  """A function that always outputs zero: f_i(x) = 0."""

  CONSTANT = 1
  """A function that outputs a constant: f_i(x) = c. The arguments for this
  function should be a list of size 1 containing the constant: [c]."""

  RADIAL_HASH = 2
  """A non-stationary function whose intensity varies as a function of
  distance to the origin. This function uses a mixing step of a hash function
  to induce pseudorandomness in this variation of intensity. More precisely:
    f_i(x) = c - k * M'(||x|| / s + D)
  where c, k, s, and D are constants, M(.) is the last mixing step of the
  32-bit MurmurHash function, and M' is the extension of M to real-valued
  inputs by linear interpolation at the two nearest integers:
    M'(t) = M(floor(t)) * (1 - t + floor(t)) + M(floor(t) + 1) * (t - floor(t)).
  The arguments for this function should be
  [c, k, s, D]."""


class InteractionFunction(Enum):
  """Item interaction function used in the Gibbs sampler for map generation.
  See `jbw/energy_functions.h` for implementations of these functions."""

  ZERO = 0
  """A function that always outputs zero: f_ij(x,y) = 0."""

  PIECEWISE_BOX = 1
  """Two rectangular functions of the squared distance between x and y,
  centered at 0. The pseudocode for f_ij(x,y) looks like:

    distance = ||x - y||^2
    if (distance < l_1):
      return c_1
    elif (distance < l_2):
      return c_2
    else:
      return 0

  where l_1, l_2, c_1, and c_2 are constants. The arguments for this function
  should be [l_1, l_2, c_1, c_2]."""

  CROSS = 2
  """A function that looks like a 'cross'. f_ij(x,y) is computed:

    diff = x - y
    if (||diff||_inf <= d_1):
      if (one coordinate of diff is zero):
        return a_1
      else:
        return b_1
    elif (||diff||_inf <= d_2):
      if (one coordinate of diff is zero):
        return a_2
      else:
        return b_2
    else:
      return 0

  where d_1, a_1, b_1, d_2, a_2, and b_2 are constants. The arguments for this
  function should be [d_1, d_2, a_1, a_2, b_1, b_2]."""

  CROSS_HASH = 3
  """This function is identical to `CROSS` except the parameters `d_1` and
  `d_2` are given by:
    d_1 = c + M'(x[0] / s) and d_2 = d_1 + \delta,
  where M' is defined above in the documentation of `RADIAL_HASH`. The
  arguments for this function should be [s, c, k, \delta, a_1, a_2, b_1, b_2]. """
