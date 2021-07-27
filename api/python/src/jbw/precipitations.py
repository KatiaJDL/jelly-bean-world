from __future__ import absolute_import, division, print_function

from enum import Enum

__all__ = ['PrecipitationsFunction']

class PrecipitationsFunction(Enum):
    ZERO = 0
    """A function that always outputs zero: f_i(x, t) = 0."""

    CONSTANT = 1
    """A function that outputs a constant: f_i(x, t) = c. The arguments for this
    function should be a list of size 1 containing the constant: [c]."""

    CUSTOM = 2
    """A function constructed as an array. The arguments for this function should be
    a list of values corresponding to the values of the function at each timestep*update_frequency
    (defined in the configuration of the simulator).
    At the end of the array the evaluation starts from the beginning.
    
    It's a temporal non-stationary function."""