import reshop as rhp
import math
import numpy as np
import scipy.sparse as sp
import pytest
from pytest import approx


def init(backend=rhp.RHP_BACKEND_RHP):
    mdl = rhp.Model(backend)
    rhp.mdl_set_option(mdl, "rtol", 1e-9)
    return mdl


def init_solver(backend=rhp.RHP_BACKEND_RHP):
    mdl_solver = rhp.Model(backend)
    rhp.mdl_set_option(mdl_solver, "rtol", 1e-9)
    return mdl_solver


def test_specialfloats():
    mdl = init()
    sv = rhp.mdl_getspecialfloats(mdl)
    assert math.isnan(sv["NaN"])
    assert sv["-inf"] == -math.inf
    assert sv["+inf"] ==  math.inf


def test_variablebounds():
    mdl = init()


def bletype():
    mdl = init()

