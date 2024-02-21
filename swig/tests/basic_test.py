import reshop as rhp
import numpy as np
import scipy.sparse as sp
import pytest
from pytest import approx


def init(backend=rhp.RHP_BACKEND_RHP):
    mdl = rhp.Model(backend)
    rhp.set_option_d(mdl, "rtol", 1e-9)
    return mdl


def init_solver(backend=rhp.RHP_BACKEND_RHP):
    mdl_solver = rhp.Model(backend)
    rhp.set_option_d(mdl_solver, "rtol", 1e-9)
    return mdl_solver


def solve(mdl, mdl_solver, will_sucess=True):
    rhp.process(mdl, mdl_solver)
    rhp.solve(mdl_solver)
    model_stat = rhp.mdl_getmodelstat(mdl_solver)
    solve_stat = rhp.mdl_getsolvestat(mdl_solver)
    if will_sucess:
        assert model_stat in (1, 2)
        assert solve_stat == 1
    else:
        assert model_stat not in (1, 2)

    rhp.postprocess(mdl_solver)


def test_oneobjvar1():
    """
    Solve min x1
          s.t.   x1 >= 0
    """

    mdl = init()
    rhp.mdl_resize(mdl, 1, 0)
    v1 = rhp.add_posvars(mdl, 1)
    rhp.mdl_setobjvar(mdl, v1[0])
    rhp.mdl_setobjsense(mdl, rhp.RHP_MIN)
    mdl_solver = init_solver()

    solve(mdl, mdl_solver)

    assert rhp.mdl_getvarsval(mdl, v1) == approx((0.0,))
    assert rhp.mdl_getvarsmult(mdl, v1) == approx((1.0,))


def test_oneobjvar2():
    """
    Solve max x1
         s.t.   x1 <= 0
    """

    mdl = init()
    rhp.mdl_resize(mdl, 1, 0)
    v1 = rhp.add_negvars(mdl, 1)
    rhp.mdl_setobjvar(mdl, v1[0])
    rhp.mdl_setobjsense(mdl, rhp.RHP_MAX)
    mdl_solver = init_solver()

    solve(mdl, mdl_solver)

    assert rhp.mdl_getvarsval(mdl, v1) == approx((0.0,))
    assert rhp.mdl_getvarsmult(mdl, v1) == approx((-1.0,))


def test_oneobjvarcons1():
    """
    Solve min     x1
        x >= 0    s.t.   x1 + x2 >= 1
    """

    mdl = init()
    rhp.mdl_resize(mdl, 2, 1)
    v1 = rhp.add_posvars(mdl, 2)
    rhp.mdl_setobjvar(mdl, v1[0])
    rhp.mdl_setobjsense(mdl, rhp.RHP_MIN)

    cons = rhp.add_greaterthan_constraint(mdl)
    rhp.equ_addlin(mdl, cons, v1, (1.0, 1.0))
    rhp.mdl_setequrhs(mdl, cons, 1.0)

    mdl_solver = init_solver()

    solve(mdl, mdl_solver)

    # PATH seems buggy
    assert rhp.mdl_getvarsval(mdl, v1) == approx((0.0, 1.0), rel=1e-5)
    assert rhp.mdl_getvarsmult(mdl, v1) == approx((1.0, 0.0))
    #     assert rhp.mdl_getequval(mdl, cons) == approx(0.0, abs=1e-5)
    assert rhp.mdl_getequmult(mdl, cons) == approx(0.0)


def test_onevar():
    """
    Solve min f(x) = x1
                    s.t.   x1 >= 0
    """

    mdl = init()
    rhp.mdl_resize(mdl, 1, 1)
    v1 = rhp.add_posvars(mdl, 1)
    objequ = rhp.add_func(mdl)

    rhp.equ_addnewlvar(mdl, objequ, v1[0], 1.0)
    rhp.mdl_setequcst(mdl, objequ, 0.0)

    rhp.mdl_setobjequ(mdl, objequ)
    rhp.mdl_setobjsense(mdl, rhp.RHP_MIN)

    mdl_solver = init_solver()

    solve(mdl, mdl_solver)

    assert rhp.mdl_getvarsval(mdl, v1) == approx((0.0,))
    assert rhp.mdl_getvarsmult(mdl, v1) == approx((1.0))


def test_onevarmax():
    """
    Solve max f(x1) = -x1
                      s.t.   x1 >= 0
    """

    mdl = init()
    rhp.mdl_resize(mdl, 1, 1)
    v1 = rhp.add_posvars(mdl, 1)
    objequ = rhp.add_func(mdl)

    rhp.equ_addnewlvar(mdl, objequ, v1[0], -1.0)
    rhp.mdl_setequcst(mdl, objequ, 0.0)

    rhp.mdl_setobjequ(mdl, objequ)
    rhp.mdl_setobjsense(mdl, rhp.RHP_MAX)

    mdl_solver = init_solver()

    solve(mdl, mdl_solver)

    assert rhp.mdl_getvarsval(mdl, v1) == approx((0.0,))
    assert rhp.mdl_getvarsmult(mdl, v1) == approx((1.0,))


def test_twovars1():
    """
    Solve min f(x1) = x1
          x >= 0      s.t.   x1 + x2 >= 1
    """

    mdl = init()
    rhp.mdl_resize(mdl, 2, 2)

    v1 = rhp.add_posvars(mdl, 2)

    objequ = rhp.add_func(mdl)
    rhp.equ_addnewlvar(mdl, objequ, v1[0], 1.0)
    rhp.mdl_setequcst(mdl, objequ, 0.0)

    cons = rhp.add_greaterthan_constraint(mdl)
    rhp.equ_addlin(mdl, cons, v1, (1.0, 1.0))
    rhp.mdl_setequrhs(mdl, cons, 1.0)

    rhp.mdl_setobjequ(mdl, objequ)
    rhp.mdl_setobjsense(mdl, rhp.RHP_MIN)

    mdl_solver = init_solver()

    solve(mdl, mdl_solver)

    assert rhp.mdl_getvarsval(mdl, v1) == approx((0.0, 1.0), rel=1e-5)
    assert rhp.mdl_getvarsmult(mdl, v1) == approx((1.0, 0.0))
    #     assert rhp.mdl_getequval(mdl, cons) == approx(0.0, abs=1e-5)
    assert rhp.mdl_getequmult(mdl, cons) == approx(0.0)


def test_qp1dense():
    """
    Solve min    2(x1^2 + x2^2)
                 s.t.   x1 + x2 >= 1
    """

    mdl = init()
    rhp.mdl_resize(mdl, 2, 2)

    v1 = rhp.add_vars(mdl, 2)

    cons = rhp.add_greaterthan_constraint(mdl)
    rhp.equ_addlin(mdl, cons, v1, (1.0, 1.0))
    rhp.mdl_setequrhs(mdl, cons, 1.0)

    objequ = rhp.add_func(mdl)
    rhp.equ_addquad(mdl, objequ, v1, v1, np.eye(2), 2.0)

    rhp.mdl_setobjequ(mdl, objequ)
    rhp.mdl_setobjsense(mdl, rhp.RHP_MIN)

    mdl_solver = init_solver()

    solve(mdl, mdl_solver)

    assert rhp.mdl_getvarsval(mdl, v1) == approx((0.5, 0.5))
    assert rhp.mdl_getvarsmult(mdl, v1) == approx((0.0, 0.0))
    #     assert rhp.mdl_getequval(mdl, cons) == approx(0.0)
    assert rhp.mdl_getequmult(mdl, cons) == approx(2.0)


def test_qp1():
    """
    Solve min    2(x1^2 + x2^2)
                 s.t.   x1 + x2 >= 1
    """

    mdl = init()
    rhp.mdl_resize(mdl, 2, 2)

    v1 = rhp.add_vars(mdl, 2)

    cons = rhp.add_greaterthan_constraint(mdl)
    rhp.equ_addlin(mdl, cons, v1, (1.0, 1.0))
    rhp.mdl_setequrhs(mdl, cons, 1.0)

    objequ = rhp.add_func(mdl)
    rhp.equ_addquad(mdl, objequ, v1, v1, sp.diags(np.ones(len(v1))), 2.0)

    rhp.mdl_setobjequ(mdl, objequ)
    rhp.mdl_setobjsense(mdl, rhp.RHP_MIN)

    mdl_solver = init_solver()

    solve(mdl, mdl_solver)

    assert rhp.mdl_getvarsval(mdl, v1) == approx((0.5, 0.5))
    assert rhp.mdl_getvarsmult(mdl, v1) == approx((0.0, 0.0))
    #     assert rhp.mdl_getequval(mdl, cons) == approx(0.0)
    assert rhp.mdl_getequmult(mdl, cons) == approx(2.0)


def test_qp2():
    """
    Solve min    x1^2 + x2^2
                 s.t.   x1 + x2 >= 1
    """

    mdl = init()
    rhp.mdl_resize(mdl, 2, 2)

    v1 = rhp.add_vars(mdl, 2)

    cons = rhp.add_greaterthan_constraint(mdl)
    rhp.equ_addlin(mdl, cons, v1, (1.0, 1.0))
    rhp.mdl_setequrhs(mdl, cons, 1.0)

    objequ = rhp.add_func(mdl)
    rhp.equ_addquad(mdl, objequ, v1, v1, sp.diags(np.ones(len(v1))), 1.0)

    rhp.mdl_setobjequ(mdl, objequ)
    rhp.mdl_setobjsense(mdl, rhp.RHP_MIN)

    mdl_solver = init_solver()

    solve(mdl, mdl_solver)

    assert rhp.mdl_getvarsval(mdl, v1) == approx((0.5, 0.5))
    assert rhp.mdl_getvarsmult(mdl, v1) == approx((0.0, 0.0))
    #     assert rhp.mdl_getequval(mdl, cons) == approx(0.0)
    assert rhp.mdl_getequmult(mdl, cons) == approx(1.0)


def test_qp3():
    """
    Solve max    -x1^2 - x2^2
                 s.t.   x1 + x2 >= 1
    """

    mdl = init()
    rhp.mdl_resize(mdl, 2, 2)

    x = rhp.add_vars(mdl, 2)

    cons = rhp.add_greaterthan_constraint(mdl)
    rhp.equ_addlin(mdl, cons, x, (1.0, 1.0))
    rhp.mdl_setequrhs(mdl, cons, 1.0)

    objequ = rhp.add_func(mdl)
    rhp.equ_addquad(mdl, objequ, x, x, sp.diags(np.ones(len(x))), -1.0)

    rhp.mdl_setobjequ(mdl, objequ)
    rhp.mdl_setobjsense(mdl, rhp.RHP_MAX)

    mdl_solver = init_solver()

    solve(mdl, mdl_solver)

    assert rhp.mdl_getvarsval(mdl, x) == approx((0.5, 0.5))
    assert rhp.mdl_getvarsmult(mdl, x) == approx((0.0, 0.0))
    #     assert rhp.mdl_getequval(mdl, cons) == approx(0.0)
    assert rhp.mdl_getequmult(mdl, cons) == approx(1.0)


def test_qp4():
    """
    Solve min    2*x1^2 + 2*x2^2 + 2x1*x2
                 s.t.   x1 + x2 >= 1
    """

    mdl = init()
    rhp.mdl_resize(mdl, 2, 2)

    v1 = rhp.add_vars(mdl, 2)

    cons = rhp.add_greaterthan_constraint(mdl)
    rhp.equ_addlin(mdl, cons, v1, (1.0, 1.0))
    rhp.mdl_setequrhs(mdl, cons, 1.0)

    objequ = rhp.add_func(mdl)
    rhp.equ_addquad(
        mdl, objequ, v1, v1, sp.coo_matrix(np.asarray(((2, 1), (1, 2.0)))), 1.0
    )

    rhp.mdl_setobjequ(mdl, objequ)
    rhp.mdl_setobjsense(mdl, rhp.RHP_MIN)

    mdl_solver = init_solver()

    solve(mdl, mdl_solver)

    assert rhp.mdl_getvarsval(mdl, v1) == approx((0.5, 0.5))
    assert rhp.mdl_getvarsmult(mdl, v1) == approx((0.0, 0.0))
    #     assert rhp.mdl_getequval(mdl, cons) == approx(0.0)
    assert rhp.mdl_getequmult(mdl, cons) == approx(3.0)


def test_qp5():
    """
    Solve min    2*x1^2 + 2*x2^2 + 2x1*x2 + x1
                 s.t.   x1 + x2 >= 1
    """

    mdl = init()
    rhp.mdl_resize(mdl, 2, 2)

    v1 = rhp.add_vars(mdl, 2)

    cons = rhp.add_greaterthan_constraint(mdl)
    rhp.equ_addlin(mdl, cons, v1, (1.0, 1.0))
    rhp.mdl_setequrhs(mdl, cons, 1.0)

    objequ = rhp.add_func(mdl)
    rhp.equ_addquad(
        mdl, objequ, v1, v1, sp.coo_matrix(np.asarray(((2, 1), (1, 2.0)))), 1.0
    )
    rhp.equ_addlinchk(mdl, objequ, v1, [1, 0])

    rhp.mdl_setobjequ(mdl, objequ)
    rhp.mdl_setobjsense(mdl, rhp.RHP_MIN)

    mdl_solver = init_solver()

    solve(mdl, mdl_solver)

    assert rhp.mdl_getvarsval(mdl, v1) == approx((0.25, 0.75))
    assert rhp.mdl_getvarsmult(mdl, v1) == approx((0.0, 0.0))
    #     assert rhp.mdl_getequval(mdl, cons) == approx(0.0)
    assert rhp.mdl_getequmult(mdl, cons) == approx(3.5)


def test_qp5bis():
    """
    Solve min    2*x1^2 + 2*x2^2 + 2x1*x2 + x1
                 s.t.   x1 + x2 >= 1
    """

    mdl = init()
    rhp.mdl_resize(mdl, 2, 2)

    v1 = rhp.add_vars(mdl, 2)

    cons = rhp.add_greaterthan_constraint(mdl)
    rhp.equ_addlin(mdl, cons, v1, (1.0, 1.0))
    rhp.mdl_setequrhs(mdl, cons, 1.0)

    objequ = rhp.add_func(mdl)
    rhp.equ_addlinchk(mdl, objequ, v1, [1, 0])
    rhp.equ_addquad(
        mdl, objequ, v1, v1, sp.coo_matrix(np.asarray(((2, 1), (1, 2.0)))), 1.0
    )

    rhp.mdl_setobjequ(mdl, objequ)
    rhp.mdl_setobjsense(mdl, rhp.RHP_MIN)

    mdl_solver = init_solver()

    solve(mdl, mdl_solver)

    assert rhp.mdl_getvarsval(mdl, v1) == approx((0.25, 0.75))
    assert rhp.mdl_getvarsmult(mdl, v1) == approx((0.0, 0.0))
    #     assert rhp.mdl_getequval(mdl, cons) == approx(0.0)
    assert rhp.mdl_getequmult(mdl, cons) == approx(3.5)


def test_tragedy_common():
    n_players = 5

    mdl = init()
    rhp.mdl_resize(mdl, n_players, 2 * n_players)

    mpe = rhp.empdag_newmpe(mdl)
    rhp.empdag_rootsaddmpe(mdl, mpe)

    v = rhp.add_varsinbox(mdl, n_players, 0.0, 1.0)

    sols = np.ones(n_players) / (n_players + 1.0)

    for i in range(len(v)):
        var = v[i]

        mp = rhp.empdag_newmp(mdl, rhp.RHP_MAX)
        rhp.mp_settype(mp, rhp.RHP_MP_OPT)
        rhp.mp_addvar(mp, var)

        objequ = rhp.add_func(mdl)

        # build the quadratic part
        m = sp.coo_matrix(
            (
                -np.ones(n_players),
                (i * np.ones(n_players, dtype=int), range(n_players)),
            ),
            shape=(n_players, n_players),
        )

        rhp.equ_addquad(mdl, objequ, m, 1.0)
        rhp.equ_addlvar(mdl, objequ, var, 1.0)
        rhp.equ_setcst(mdl, objequ, 0)

        rhp.mp_setobjequ(mp, objequ)

        cons = rhp.add_lessthan_constraint(mdl)
        rhp.equ_addlin(mdl, cons, v, np.ones(n_players))
        rhp.mdl_setequrhs(mdl, cons, 1)
        rhp.mp_addconstraint(mp, cons)
        #        rhp.mp_finalize(mp)

        rhp.empdag_mpeaddmp(mdl, mpe, mp)

    mdl_solver = init_solver()

    solve(mdl, mdl_solver)

    assert rhp.mdl_getvarsval(mdl, v) == approx(sols)
    assert rhp.mdl_getvarsmult(mdl, v) == approx(np.zeros(n_players), abs=1e-10)
    #     assert rhp.mdl_getequval(mdl, cons) == approx(-1.0 / (n_players + 1))
    assert rhp.mdl_getequmult(mdl, cons) == approx(0.0)


def test_cvar():
    mdl = init()

    d = 1 + np.arange(4.0)
    p = (0.125, 0.25, 0.5, 0.125)
    theta = 1 - 0.15

    rhp.mdl_resize(mdl, len(d) + 1, len(d) + 1)

    rho = rhp.add_varsnamed(mdl, 1, "rho_cvar")

    objequ = rhp.add_funcnamed(mdl, "objequ")
    rhp.equ_addlvar(mdl, objequ, rho[0], 1.0)
    rhp.mdl_setobjequ(mdl, objequ)
    rhp.mdl_setobjsense(mdl, rhp.RHP_MIN)

    v_arg_cvar = rhp.add_varsnamed(mdl, len(d), "cvar_arg")

    # set equations z[i] = d[i]
    for i in range(len(d)):
        ei = rhp.add_equality_constraint(mdl)
        rhp.equ_addnewlvar(mdl, ei, v_arg_cvar[i], 1.0)
        rhp.mdl_setequrhs(mdl, ei, d[i])

    # Start definition of the cvar
    ovf_def = rhp.ovf_add(mdl, "cvarup", rho[0], v_arg_cvar)

    # Set the tail percentage for the cvar
    rhp.ovf_param_add_scalar(ovf_def, "tail", theta)
    # Set the probabilities for each event.
    # Warning, the order in p and v_arg_cvar must be the same!
    rhp.ovf_param_add_vector(ovf_def, "probabilities", p)

    # Select which method to solve
    rhp.ovf_setreformulation(ovf_def, "fenchel")

    # See if we got it right
    rhp.ovf_check(mdl, ovf_def)

    mdl_solver = init_solver()

    solve(mdl, mdl_solver)

    assert rhp.mdl_getvarsval(mdl, rho) == approx(4.0 - 1 / 6.0)


def test_cvarlo():
    mdl = init()

    d = 1 + np.arange(4.0)
    p = (0.125, 0.25, 0.5, 0.125)
    theta = 1 - 0.15

    rhp.mdl_resize(mdl, len(d) + 1, len(d) + 1)

    rho = rhp.add_vars(mdl, 1)

    objequ = rhp.add_func(mdl)
    rhp.equ_addlvar(mdl, objequ, rho[0], 1.0)
    rhp.mdl_setobjequ(mdl, objequ)
    rhp.mdl_setobjsense(mdl, rhp.RHP_MAX)

    v_arg_cvar = rhp.add_vars(mdl, len(d))

    # set equations z[i] = d[i]
    for i in range(len(d)):
        ei = rhp.add_equality_constraint(mdl)
        rhp.equ_addnewlvar(mdl, ei, v_arg_cvar[i], 1.0)
        rhp.mdl_setequrhs(mdl, ei, d[i])

    # Start definition of the cvar
    ovf_def = rhp.ovf_add(mdl, "cvarlo", rho[0], v_arg_cvar)

    # Set the tail percentage for the cvar
    rhp.ovf_param_add_scalar(ovf_def, "tail", theta)
    # Set the probabilities for each event.
    # Warning, the order in p and v_arg_cvar must be the same!
    rhp.ovf_param_add_vector(ovf_def, "probabilities", p)

    # Select which method to solve
    rhp.ovf_setreformulation(ovf_def, "fenchel")

    # See if we got it right
    rhp.ovf_check(mdl, ovf_def)

    mdl_solver = init_solver()

    solve(mdl, mdl_solver)

    assert rhp.mdl_getvarsval(mdl, rho) == approx(
        ((0.125 * d[0] + 0.025 * d[1]) / (1 - theta),)
    )


def test_cvarcons():
    mdl = init()

    d = 1 + np.arange(4.0)
    p = (0.125, 0.25, 0.5, 0.125)
    theta = 1 - 0.15

    rhp.mdl_resize(mdl, len(d) + 2, len(d) + 2)

    x = rhp.add_posvars(mdl, 1)
    objequ = rhp.add_func(mdl)
    rhp.equ_addlvar(mdl, objequ, x[0], 1.0)
    rhp.mdl_setobjequ(mdl, objequ)
    rhp.mdl_setobjsense(mdl, rhp.RHP_MIN)

    rho = rhp.add_vars(mdl, 1)
    cons = rhp.add_lessthan_constraint(mdl)
    rhp.equ_addlvar(mdl, cons, rho[0], 1.0)
    rhp.mdl_setequrhs(mdl, cons, 4 - 1 / 6.0 + 1e-6)

    v_arg_cvar = rhp.add_vars(mdl, len(d))

    # set equations z[i] = d[i]
    for i in range(len(d)):
        ei = rhp.add_equality_constraint(mdl)
        rhp.equ_addnewlvar(mdl, ei, v_arg_cvar[i], 1.0)
        rhp.mdl_setequrhs(mdl, ei, d[i])

    # Start definition of the cvar
    ovf_def = rhp.ovf_add(mdl, "cvarup", rho[0], v_arg_cvar)

    # Set the tail percentage for the cvar
    rhp.ovf_param_add_scalar(ovf_def, "tail", theta)
    # Set the probabilities for each event.
    # Warning, the order in p and v_arg_cvar must be the same!
    rhp.ovf_param_add_vector(ovf_def, "probabilities", p)

    # Select which method to solve
    rhp.ovf_setreformulation(ovf_def, "fenchel")

    # See if we got it right
    rhp.ovf_check(mdl, ovf_def)

    mdl_solver = init_solver()

    solve(mdl, mdl_solver)

    assert rhp.mdl_getvarsval(mdl, rho) == approx(4.0 - 1 / 6.0, rel=1e-3)
    assert rhp.mdl_getvarsval(mdl, x) == approx(0)


def test_cvarconsfail():
    mdl = init()

    d = 1 + np.arange(4.0)
    p = (0.125, 0.25, 0.5, 0.125)
    theta = 1 - 0.15

    rhp.mdl_resize(mdl, len(d) + 2, len(d) + 2)

    x = rhp.add_posvars(mdl, 1)
    objequ = rhp.add_func(mdl)
    rhp.equ_addlvar(mdl, objequ, x[0], 1.0)
    rhp.mdl_setobjequ(mdl, objequ)
    rhp.mdl_setobjsense(mdl, rhp.RHP_MIN)

    rho = rhp.add_vars(mdl, 1)
    cons = rhp.add_lessthan_constraint(mdl)
    rhp.equ_addlvar(mdl, cons, rho[0], 1.0)
    rhp.mdl_setequrhs(mdl, cons, 3.5)

    v_arg_cvar = rhp.add_vars(mdl, len(d))

    # set equations z[i] = d[i]
    for i in range(len(d)):
        ei = rhp.add_equality_constraint(mdl)
        rhp.equ_addnewlvar(mdl, ei, v_arg_cvar[i], 1.0)
        rhp.mdl_setequcst(mdl, ei, -d[i])

    # Start definition of the cvar
    ovf_def = rhp.ovf_add(mdl, "cvarup", rho[0], v_arg_cvar)

    # Set the tail percentage for the cvar
    rhp.ovf_param_add_scalar(ovf_def, "tail", theta)
    # Set the probabilities for each event.
    # Warning, the order in p and v_arg_cvar must be the same!
    rhp.ovf_param_add_vector(ovf_def, "probabilities", p)

    # Select which method to solve
    rhp.ovf_setreformulation(ovf_def, "fenchel")

    # See if we got it right
    rhp.ovf_check(mdl, ovf_def)

    mdl_solver = init_solver()

    solve(mdl, mdl_solver, will_sucess=False)
