#ifndef _PLM_H_
#define _PLM_H_

#include <cmath>

#include <AMReX_FArrayBox.H>
#include <AMReX_IArrayBox.H>

#include "Constants.H"
#include "IndexDefines.H"
#include "EOS.H"
#include "Riemann.H"

/*This header file contains functions and declarations for the 3D hydro portion
  of PeleC cuda. As per the convention of AMReX, inlined functions are defined
  here. Where as non-inline functions are declared here. */

AMREX_GPU_DEVICE
AMREX_FORCE_INLINE
amrex::Real
plm_slope(
  const int i,
  const int j,
  const int k,
  const int n,
  const int dir,
  amrex::Array4<const amrex::Real> const& q)
{
  const int bdim[3] = {dir == 0, dir == 1, dir == 2};

  amrex::Real dlft, drgt, dcen, dfm, dfp, dlim, dsgn, dtemp;
  const amrex::Real qc = q(i, j, k, n);
  const amrex::Real qm =
    q(i - 1 * bdim[0], j - 1 * bdim[1], k - 1 * bdim[2], n);
  const amrex::Real qp =
    q(i + 1 * bdim[0], j + 1 * bdim[1], k + 1 * bdim[2], n);

  dlft = qm - q(i - 2 * bdim[0], j - 2 * bdim[1], k - 2 * bdim[2], n);
  drgt = qc - qm;
  dcen = 0.5 * (dlft + drgt);
  dsgn = amrex::Math::copysign(1.0, dcen);
  dlim = (dlft * drgt >= 0.0)
           ? 2.0 * amrex::min(amrex::Math::abs(dlft), amrex::Math::abs(drgt))
           : 0.0;
  dfm = dsgn * amrex::min(dlim, amrex::Math::abs(dcen));

  dlft = qp - qc;
  drgt = q(i + 2 * bdim[0], j + 2 * bdim[1], k + 2 * bdim[2], n) - qp;
  dcen = 0.5 * (dlft + drgt);
  dsgn = amrex::Math::copysign(1.0, dcen);
  dlim = (dlft * drgt >= 0.0)
           ? 2.0 * amrex::min(amrex::Math::abs(dlft), amrex::Math::abs(drgt))
           : 0.0;
  dfp = dsgn * amrex::min(dlim, amrex::Math::abs(dcen));

  dlft = qc - qm;
  drgt = qp - qc;
  dcen = 0.5 * (dlft + drgt);
  dsgn = amrex::Math::copysign(1.0, dcen);
  dlim = (dlft * drgt >= 0.0)
           ? 2.0 * amrex::min(amrex::Math::abs(dlft), amrex::Math::abs(drgt))
           : 0.0;
  dcen = dsgn * amrex::min(dlim, amrex::Math::abs(dcen));

  dtemp = 4.0 / 3.0 * dcen - 1.0 / 6.0 * (dfp + dfm);

  // Flattening could be done here (see Nyx if we want to do it)
  return dsgn * amrex::min(dlim, amrex::Math::abs(dtemp));
}

AMREX_GPU_DEVICE
AMREX_FORCE_INLINE
void
pc_plm_x(
  const int i,
  const int j,
  const int k,
  amrex::Array4<amrex::Real> const& qm,
  amrex::Array4<amrex::Real> const& qp,
  amrex::Real const slope[QVAR],
  amrex::Array4<const amrex::Real> const& q,
  amrex::Real const cc,
  // amrex::Array4<const amrex::Real> const& dloga,
  amrex::Real const dx,
  amrex::Real const dt)
{
  amrex::Real e[3];
  amrex::Real dtdx = dt / dx;
  // amrex::Real cc = qa(i,j,k,QC);
  amrex::Real cs = cc * cc;
  amrex::Real rho = q(i, j, k, QRHO);
  amrex::Real u = q(i, j, k, QU);
  amrex::Real v = q(i, j, k, QV);
  amrex::Real w = q(i, j, k, QW);
  amrex::Real p = q(i, j, k, QPRES);
  amrex::Real rhoe = q(i, j, k, QREINT);
  amrex::Real enth = ((rhoe + p) / rho) / cs;
  amrex::Real drho = slope[QRHO];
  amrex::Real du = slope[QU];
  amrex::Real dv = slope[QV];
  amrex::Real dw = slope[QW];
  amrex::Real dp = slope[QPRES];
  amrex::Real drhoe = slope[QREINT];
  amrex::Real alpham = 0.5 * (dp / (rho * cc) - du) * rho / cc;
  amrex::Real alphap = 0.5 * (dp / (rho * cc) + du) * rho / cc;
  amrex::Real alpha0r = drho - dp / cs;
  amrex::Real alpha0e = drhoe - dp * enth;
  amrex::Real alpha0v = dv;
  amrex::Real alpha0w = dw;

  e[0] = u - cc;
  e[1] = u;
  e[2] = u + cc;
  //  Construct the right state on the i-1/2 interface
  amrex::Real rho_ref = rho - 0.5 * (1.0 + dtdx * amrex::min(e[0], 0.0)) * drho;
  amrex::Real u_ref = u - 0.5 * (1.0 + dtdx * amrex::min(e[0], 0.0)) * du;
  amrex::Real v_ref = v - 0.5 * (1.0 + dtdx * amrex::min(e[0], 0.0)) * dv;
  amrex::Real w_ref = w - 0.5 * (1.0 + dtdx * amrex::min(e[0], 0.0)) * dw;
  amrex::Real p_ref = p - 0.5 * (1.0 + dtdx * amrex::min(e[0], 0.0)) * dp;
  amrex::Real rhoe_ref =
    rhoe - 0.5 * (1.0 + dtdx * amrex::min(e[0], 0.0)) * drhoe;

  amrex::Real apright = 0.25 * dtdx * (e[0] - e[2]) *
                        (1.0 - amrex::Math::copysign(1.0, e[2])) * alphap;
  //                  amright = 0.25*dtdx*(e[0] - e[0])*(1.0 -
  //                  amrex::Math::copysign(1.0, e[0]))*alpham;
  amrex::Real amright = 0.0;

  amrex::Real azrright = 0.25 * dtdx * (e[0] - e[1]) *
                         (1.0 - amrex::Math::copysign(1.0, e[1])) * alpha0r;
  amrex::Real azeright = 0.25 * dtdx * (e[0] - e[1]) *
                         (1.0 - amrex::Math::copysign(1.0, e[1])) * alpha0e;
  amrex::Real azv1rght = 0.25 * dtdx * (e[0] - e[1]) *
                         (1.0 - amrex::Math::copysign(1.0, e[1])) * alpha0v;
  amrex::Real azw1rght = 0.25 * dtdx * (e[0] - e[1]) *
                         (1.0 - amrex::Math::copysign(1.0, e[1])) * alpha0w;

  qp(i, j, k, QRHO) = rho_ref + apright + amright + azrright;
  qp(i, j, k, QRHO) = amrex::max(SMALL_DENS, qp(i, j, k, QRHO));
  AMREX_D_TERM(qp(i, j, k, QU) = u_ref + (apright - amright) * cc / rho;
               qp(i, j, k, QV) = 0.; qp(i, j, k, QW) = 0.;
               , qp(i, j, k, QV) = v_ref + azv1rght;
               , qp(i, j, k, QW) = w_ref + azw1rght;);
  qp(i, j, k, QPRES) = p_ref + (apright + amright) * cs;
  qp(i, j, k, QPRES) = amrex::max(qp(i, j, k, QPRES), SMALL_PRES);
  qp(i, j, k, QREINT) = rhoe_ref + (apright + amright) * enth * cs + azeright;

  //  Construct the left state on the i+1/2 interface

  rho_ref = rho + 0.5 * (1.0 - dtdx * amrex::max(e[2], 0.0)) * drho;
  u_ref = u + 0.5 * (1.0 - dtdx * amrex::max(e[2], 0.0)) * du;
  v_ref = v + 0.5 * (1.0 - dtdx * amrex::max(e[2], 0.0)) * dv;
  w_ref = w + 0.5 * (1.0 - dtdx * amrex::max(e[2], 0.0)) * dw;
  p_ref = p + 0.5 * (1.0 - dtdx * amrex::max(e[2], 0.0)) * dp;
  rhoe_ref = rhoe + 0.5 * (1.0 - dtdx * amrex::max(e[2], 0.0)) * drhoe;

  //                  apleft = 0.25*dtdx*(e[2] - e[2])*(1.0 +
  //                  amrex::Math::copysign(1.0,e[2]))*alphap;
  amrex::Real apleft = 0.0;
  amrex::Real amleft = 0.25 * dtdx * (e[2] - e[0]) *
                       (1.0 + amrex::Math::copysign(1.0, e[0])) * alpham;

  amrex::Real azrleft = 0.25 * dtdx * (e[2] - e[1]) *
                        (1.0 + amrex::Math::copysign(1.0, e[1])) * alpha0r;
  amrex::Real azeleft = 0.25 * dtdx * (e[2] - e[1]) *
                        (1.0 + amrex::Math::copysign(1.0, e[1])) * alpha0e;
  amrex::Real azv1left = 0.25 * dtdx * (e[2] - e[1]) *
                         (1.0 + amrex::Math::copysign(1.0, e[1])) * alpha0v;
  amrex::Real azw1left = 0.25 * dtdx * (e[2] - e[1]) *
                         (1.0 + amrex::Math::copysign(1.0, e[1])) * alpha0w;
  qm(i + 1, j, k, QRHO) = rho_ref + apleft + amleft + azrleft;
  qm(i + 1, j, k, QRHO) = amrex::max(qm(i + 1, j, k, QRHO), SMALL_DENS);
  AMREX_D_TERM(qm(i + 1, j, k, QU) = u_ref + (apleft - amleft) * cc / rho;
               qm(i + 1, j, k, QV) = 0.; qm(i + 1, j, k, QW) = 0.;
               , qm(i + 1, j, k, QV) = v_ref + azv1left;
               , qm(i + 1, j, k, QW) = w_ref + azw1left;);
  qm(i + 1, j, k, QPRES) = p_ref + (apleft + amleft) * cs;
  qm(i + 1, j, k, QPRES) = amrex::max(qm(i + 1, j, k, QPRES), SMALL_PRES);
  qm(i + 1, j, k, QREINT) = rhoe_ref + (apleft + amleft) * enth * cs + azeleft;
  // Upwind the passive variables
  for (int ipassive = 0; ipassive < NPASSIVE; ++ipassive) {
    int n = indxmap::qpass_map(ipassive);
    // Right state
    u = q(i, j, k, QU);
    amrex::Real spzero;
    spzero = u > 0 ? -1.0 : u * dtdx;
    amrex::Real acmprght = 0.5 * (-1.0 - spzero) * slope[n];
    qp(i, j, k, n) = q(i, j, k, n) + acmprght;

    spzero = u >= 0 ? u * dtdx : 1.0;
    amrex::Real acmpleft = 0.5 * (1.0 - spzero) * slope[n];
    qm(i + 1, j, k, n) = q(i, j, k, n) + acmpleft;
  }
}

AMREX_GPU_DEVICE
AMREX_FORCE_INLINE
void
pc_plm_y(
  const int i,
  const int j,
  const int k,
  amrex::Array4<amrex::Real> const& qm,
  amrex::Array4<amrex::Real> const& qp,
  amrex::Real const slope[QVAR],
  amrex::Array4<const amrex::Real> const& q,
  amrex::Real const cc,
  amrex::Real const dy,
  amrex::Real const dt)
{
  amrex::Real e[3];
  amrex::Real dtdy = dt / dy;
  amrex::Real cs = cc * cc;
  amrex::Real rho = q(i, j, k, QRHO);
  amrex::Real u = q(i, j, k, QU);
  amrex::Real v = q(i, j, k, QV);
  amrex::Real w = q(i, j, k, QW);
  amrex::Real p = q(i, j, k, QPRES);
  amrex::Real rhoe = q(i, j, k, QREINT);
  amrex::Real enth = ((rhoe + p) / rho) / cs;
  amrex::Real drho = slope[QRHO];
  amrex::Real du = slope[QU];
  amrex::Real dv = slope[QV];
  amrex::Real dw = slope[QW];
  amrex::Real dp = slope[QPRES];
  amrex::Real drhoe = slope[QREINT];
  amrex::Real alpham = 0.5 * (dp / (rho * cc) - dv) * rho / cc;
  amrex::Real alphap = 0.5 * (dp / (rho * cc) + dv) * rho / cc;
  amrex::Real alpha0r = drho - dp / cs;
  amrex::Real alpha0e = drhoe - dp * enth;
  amrex::Real alpha0u = du;
  amrex::Real alpha0w = dw;

  e[0] = v - cc;
  e[1] = v;
  e[2] = v + cc;
  // Construct the "right" state on the j-1/2 interface
  amrex::Real rho_ref = rho - 0.5 * (1.0 + dtdy * amrex::min(e[0], 0.0)) * drho;
  amrex::Real u_ref = u - 0.5 * (1.0 + dtdy * amrex::min(e[0], 0.0)) * du;
  amrex::Real v_ref = v - 0.5 * (1.0 + dtdy * amrex::min(e[0], 0.0)) * dv;
  amrex::Real w_ref = w - 0.5 * (1.0 + dtdy * amrex::min(e[0], 0.0)) * dw;
  amrex::Real p_ref = p - 0.5 * (1.0 + dtdy * amrex::min(e[0], 0.0)) * dp;
  amrex::Real rhoe_ref =
    rhoe - 0.5 * (1.0 + dtdy * amrex::min(e[0], 0.0)) * drhoe;

  amrex::Real apright = 0.25 * dtdy * (e[0] - e[2]) *
                        (1.0 - amrex::Math::copysign(1.0, e[2])) * alphap;
  //     amright = 0.25*dtdy*(e[0] - e[0])*(1.0 - amrex::Math::copysign(1.0,
  //     e[0]))*alpham;
  amrex::Real amright = 0.0;

  amrex::Real azrright = 0.25 * dtdy * (e[0] - e[1]) *
                         (1.0 - amrex::Math::copysign(1.0, e[1])) * alpha0r;
  amrex::Real azeright = 0.25 * dtdy * (e[0] - e[1]) *
                         (1.0 - amrex::Math::copysign(1.0, e[1])) * alpha0e;
  amrex::Real azu1rght = 0.25 * dtdy * (e[0] - e[1]) *
                         (1.0 - amrex::Math::copysign(1.0, e[1])) * alpha0u;
  amrex::Real azw1rght = 0.25 * dtdy * (e[0] - e[1]) *
                         (1.0 - amrex::Math::copysign(1.0, e[1])) * alpha0w;

  qp(i, j, k, QRHO) = rho_ref + apright + amright + azrright;
  qp(i, j, k, QRHO) = amrex::max(SMALL_DENS, qp(i, j, k, QRHO));
  qp(i, j, k, QU) = u_ref + azu1rght;
  qp(i, j, k, QV) = v_ref + (apright - amright) * cc / rho;
  AMREX_D_TERM(qp(i, j, k, QW) = 0.;, , qp(i, j, k, QW) = w_ref + azw1rght;);
  qp(i, j, k, QPRES) = p_ref + (apright + amright) * cs;
  qp(i, j, k, QPRES) = amrex::max(qp(i, j, k, QPRES), SMALL_PRES);
  qp(i, j, k, QREINT) = rhoe_ref + (apright + amright) * enth * cs + azeright;

  // Construct the "left" state on the j+1/2 interface

  rho_ref = rho + 0.5 * (1.0 - dtdy * amrex::max(e[2], 0.0)) * drho;
  u_ref = u + 0.5 * (1.0 - dtdy * amrex::max(e[2], 0.0)) * du;
  v_ref = v + 0.5 * (1.0 - dtdy * amrex::max(e[2], 0.0)) * dv;
  w_ref = w + 0.5 * (1.0 - dtdy * amrex::max(e[2], 0.0)) * dw;
  p_ref = p + 0.5 * (1.0 - dtdy * amrex::max(e[2], 0.0)) * dp;
  rhoe_ref = rhoe + 0.5 * (1.0 - dtdy * amrex::max(e[2], 0.0)) * drhoe;

  //                 apleft = 0.25*dtdy*(e[2] - e[2])*(1.0 +
  //                 amrex::Math::copysign(1.0,e[2]))*alphap;
  amrex::Real apleft = 0.0;
  amrex::Real amleft = 0.25 * dtdy * (e[2] - e[0]) *
                       (1.0 + amrex::Math::copysign(1.0, e[0])) * alpham;

  amrex::Real azrleft = 0.25 * dtdy * (e[2] - e[1]) *
                        (1.0 + amrex::Math::copysign(1.0, e[1])) * alpha0r;
  amrex::Real azeleft = 0.25 * dtdy * (e[2] - e[1]) *
                        (1.0 + amrex::Math::copysign(1.0, e[1])) * alpha0e;
  amrex::Real azu1left = 0.25 * dtdy * (e[2] - e[1]) *
                         (1.0 + amrex::Math::copysign(1.0, e[1])) * alpha0u;
  amrex::Real azw1left = 0.25 * dtdy * (e[2] - e[1]) *
                         (1.0 + amrex::Math::copysign(1.0, e[1])) * alpha0w;

  qm(i, j + 1, k, QRHO) = rho_ref + apleft + amleft + azrleft;
  qm(i, j + 1, k, QRHO) = amrex::max(qm(i, j + 1, k, QRHO), SMALL_DENS);
  qm(i, j + 1, k, QU) = u_ref + azu1left;
  qm(i, j + 1, k, QV) = v_ref + (apleft - amleft) * cc / rho;
  AMREX_D_TERM(qm(i, j + 1, k, QW) = 0.;
               , , qm(i, j + 1, k, QW) = w_ref + azw1left;);
  qm(i, j + 1, k, QPRES) = p_ref + (apleft + amleft) * cs;
  qm(i, j + 1, k, QPRES) = amrex::max(qm(i, j + 1, k, QPRES), SMALL_PRES);
  qm(i, j + 1, k, QREINT) = rhoe_ref + (apleft + amleft) * enth * cs + azeleft;

  // Upwind the passive variables
  for (int ipassive = 0; ipassive < NPASSIVE; ++ipassive) {
    int n = indxmap::qpass_map(ipassive);
    // Right state
    v = q(i, j, k, QV);
    amrex::Real spzero;
    spzero = v > 0 ? -1.0 : v * dtdy;
    amrex::Real acmptop = 0.5 * (-1.0 - spzero) * slope[n];
    qp(i, j, k, n) = q(i, j, k, n) + acmptop;

    spzero = v >= 0 ? v * dtdy : 1.0;
    amrex::Real acmpbot = 0.5 * (1.0 - spzero) * slope[n];
    qm(i, j + 1, k, n) = q(i, j, k, n) + acmpbot;
  }
}

AMREX_GPU_DEVICE
AMREX_FORCE_INLINE
void
pc_plm_z(
  const int i,
  const int j,
  const int k,
  amrex::Array4<amrex::Real> const& qm,
  amrex::Array4<amrex::Real> const& qp,
  amrex::Real const slope[QVAR],
  amrex::Array4<const amrex::Real> const& q,
  amrex::Real const cc,
  amrex::Real const dz,
  amrex::Real const dt)
{
  amrex::Real e[3];
  amrex::Real dtdz = dt / dz;
  amrex::Real cs = cc * cc;
  amrex::Real rho = q(i, j, k, QRHO);
  amrex::Real u = q(i, j, k, QU);
  amrex::Real v = q(i, j, k, QV);
  amrex::Real w = q(i, j, k, QW);
  amrex::Real p = q(i, j, k, QPRES);
  amrex::Real rhoe = q(i, j, k, QREINT);
  amrex::Real enth = ((rhoe + p) / rho) / cs;
  amrex::Real drho = slope[QRHO];
  amrex::Real du = slope[QU];
  amrex::Real dv = slope[QV];
  amrex::Real dw = slope[QW];
  amrex::Real dp = slope[QPRES];
  amrex::Real drhoe = slope[QREINT];
  amrex::Real alpham = 0.5 * (dp / (rho * cc) - dw) * rho / cc;
  amrex::Real alphap = 0.5 * (dp / (rho * cc) + dw) * rho / cc;
  amrex::Real alpha0r = drho - dp / cs;
  amrex::Real alpha0e = drhoe - dp * enth;
  amrex::Real alpha0u = du;
  amrex::Real alpha0v = dv;

  e[0] = w - cc;
  e[1] = w;
  e[2] = w + cc;

  // Construct the "right" state on the k-1/2 interface
  amrex::Real rho_ref = rho - 0.5 * (1.0 + dtdz * amrex::min(e[0], 0.0)) * drho;
  amrex::Real u_ref = u - 0.5 * (1.0 + dtdz * amrex::min(e[0], 0.0)) * du;
  amrex::Real v_ref = v - 0.5 * (1.0 + dtdz * amrex::min(e[0], 0.0)) * dv;
  amrex::Real w_ref = w - 0.5 * (1.0 + dtdz * amrex::min(e[0], 0.0)) * dw;
  amrex::Real p_ref = p - 0.5 * (1.0 + dtdz * amrex::min(e[0], 0.0)) * dp;
  amrex::Real rhoe_ref =
    rhoe - 0.5 * (1.0 + dtdz * amrex::min(e[0], 0.0)) * drhoe;

  amrex::Real apright = 0.25 * dtdz * (e[0] - e[2]) *
                        (1.0 - amrex::Math::copysign(1.0, e[2])) * alphap;
  //     amright = 0.25*dtdz*(e[0] - e[0])*(1.0 - amrex::Math::copysign(1.0,
  //     e[0]))*alpham;
  amrex::Real amright = 0.0;

  amrex::Real azrright = 0.25 * dtdz * (e[0] - e[1]) *
                         (1.0 - amrex::Math::copysign(1.0, e[1])) * alpha0r;
  amrex::Real azeright = 0.25 * dtdz * (e[0] - e[1]) *
                         (1.0 - amrex::Math::copysign(1.0, e[1])) * alpha0e;
  amrex::Real azu1rght = 0.25 * dtdz * (e[0] - e[1]) *
                         (1.0 - amrex::Math::copysign(1.0, e[1])) * alpha0u;
  amrex::Real azv1rght = 0.25 * dtdz * (e[0] - e[1]) *
                         (1.0 - amrex::Math::copysign(1.0, e[1])) * alpha0v;

  qp(i, j, k, QRHO) = rho_ref + apright + amright + azrright;
  qp(i, j, k, QRHO) = amrex::max(SMALL_DENS, qp(i, j, k, QRHO));
  qp(i, j, k, QU) = u_ref + azu1rght;
  qp(i, j, k, QV) = v_ref + azv1rght;
  qp(i, j, k, QW) = w_ref + (apright - amright) * cc / rho;
  qp(i, j, k, QPRES) = p_ref + (apright + amright) * cs;
  qp(i, j, k, QPRES) = amrex::max(qp(i, j, k, QPRES), SMALL_PRES);
  qp(i, j, k, QREINT) = rhoe_ref + (apright + amright) * enth * cs + azeright;

  // Construct the "left" state on the j+1/2 interface

  rho_ref = rho + 0.5 * (1.0 - dtdz * amrex::max(e[2], 0.0)) * drho;
  u_ref = u + 0.5 * (1.0 - dtdz * amrex::max(e[2], 0.0)) * du;
  v_ref = v + 0.5 * (1.0 - dtdz * amrex::max(e[2], 0.0)) * dv;
  w_ref = w + 0.5 * (1.0 - dtdz * amrex::max(e[2], 0.0)) * dw;
  p_ref = p + 0.5 * (1.0 - dtdz * amrex::max(e[2], 0.0)) * dp;
  rhoe_ref = rhoe + 0.5 * (1.0 - dtdz * amrex::max(e[2], 0.0)) * drhoe;

  //                 apleft = 0.25*dtdz*(e[2] - e[2])*(1.0 +
  //                 amrex::Math::copysign(1.0,e[2]))*alphap;
  amrex::Real apleft = 0.0;
  amrex::Real amleft = 0.25 * dtdz * (e[2] - e[0]) *
                       (1.0 + amrex::Math::copysign(1.0, e[0])) * alpham;

  amrex::Real azrleft = 0.25 * dtdz * (e[2] - e[1]) *
                        (1.0 + amrex::Math::copysign(1.0, e[1])) * alpha0r;
  amrex::Real azeleft = 0.25 * dtdz * (e[2] - e[1]) *
                        (1.0 + amrex::Math::copysign(1.0, e[1])) * alpha0e;
  amrex::Real azu1left = 0.25 * dtdz * (e[2] - e[1]) *
                         (1.0 + amrex::Math::copysign(1.0, e[1])) * alpha0u;
  amrex::Real azv1left = 0.25 * dtdz * (e[2] - e[1]) *
                         (1.0 + amrex::Math::copysign(1.0, e[1])) * alpha0v;

  qm(i, j, k + 1, QRHO) = rho_ref + apleft + amleft + azrleft;
  qm(i, j, k + 1, QRHO) = amrex::max(qm(i, j, k + 1, QRHO), SMALL_DENS);
  qm(i, j, k + 1, QU) = u_ref + azu1left;
  qm(i, j, k + 1, QV) = v_ref + azv1left;
  qm(i, j, k + 1, QW) = w_ref + (apleft - amleft) * cc / rho;
  qm(i, j, k + 1, QPRES) = p_ref + (apleft + amleft) * cs;
  qm(i, j, k + 1, QPRES) = amrex::max(qm(i, j, k + 1, QPRES), SMALL_PRES);
  qm(i, j, k + 1, QREINT) = rhoe_ref + (apleft + amleft) * enth * cs + azeleft;

  // Upwind the passive variables
  for (int ipassive = 0; ipassive < NPASSIVE; ++ipassive) {
    int n = indxmap::qpass_map(ipassive);
    // Right state
    w = q(i, j, k, QW);
    amrex::Real spzero;
    spzero = w > 0 ? -1.0 : w * dtdz;
    amrex::Real acmptop = 0.5 * (-1.0 - spzero) * slope[n];
    qp(i, j, k, n) = q(i, j, k, n) + acmptop;

    spzero = w >= 0 ? w * dtdz : 1.0;
    amrex::Real acmpbot = 0.5 * (1.0 - spzero) * slope[n];
    qm(i, j, k + 1, n) = q(i, j, k, n) + acmpbot;
  }
}

// Corrects the left and right Y-states with the X-Flux
AMREX_GPU_DEVICE
AMREX_FORCE_INLINE
void
pc_transx(
  const int i,
  const int j,
  const int k,
  amrex::Array4<amrex::Real> const& qm,
  amrex::Array4<amrex::Real> const& qp,
  amrex::Array4<const amrex::Real> const& qym,
  amrex::Array4<const amrex::Real> const& qyp,
  amrex::Array4<const amrex::Real> const& flxx,
  amrex::Array4<const amrex::Real> const& srcQ,
  amrex::Array4<const amrex::Real> const& qa,
  amrex::Array4<const amrex::Real> const& qint,
  amrex::Array4<const amrex::Real> const& a1,
  amrex::Array4<const amrex::Real> const& vol,
  const amrex::Real hdt)
{
  //  Local variables to avoid reading from global memory too much
  amrex::Real compn, rr, rrnew, compo, c;
  amrex::Real rrr, rur, rvr, ekinr, rer, rrnewr;
  amrex::Real runewr, rvnewr, renewr, rhoekinr;
  amrex::Real rrl, rul, rvl, ekinl, rel, rrnewl, runewl, rvnewl, renewl;
  amrex::Real rhoekinl, srcr, srce, srcp, srcpass;
  amrex::Real pggp, pggm, ugp, ugm;
  amrex::Real dAup, pav, dAu;
  amrex::Real flxrho, flxu, flxv, flxe;

  flxrho = hdt *
           (a1(i + 1, j, k) * flxx(i + 1, j, k, URHO) -
            a1(i, j, k) * flxx(i, j, k, URHO)) /
           vol(i, j, k);
  flxu = hdt *
         (a1(i + 1, j, k) * flxx(i + 1, j, k, UMX) -
          a1(i, j, k) * flxx(i, j, k, UMX)) /
         vol(i, j, k);
  flxv = hdt *
         (a1(i + 1, j, k) * flxx(i + 1, j, k, UMY) -
          a1(i, j, k) * flxx(i, j, k, UMY)) /
         vol(i, j, k);
  flxe = hdt *
         (a1(i + 1, j, k) * flxx(i + 1, j, k, UEDEN) -
          a1(i, j, k) * flxx(i, j, k, UEDEN)) /
         vol(i, j, k);
  srcr = srcQ(i, j, k, QRHO);
  srce = srcQ(i, j, k, QREINT);
  srcp = srcQ(i, j, k, QPRES);
  c = qa(i, j, k, QGAMC);
  // Update passive variables
  for (int ipass = 0; ipass < NPASSIVE; ++ipass) {
    int n = indxmap::upass_map(ipass);
    int nqp = indxmap::qpass_map(ipass);
    srcpass = srcQ(i, j, k, nqp);
    compn = hdt *
            (a1(i + 1, j, k) * flxx(i + 1, j, k, n) -
             a1(i, j, k) * flxx(i, j, k, n)) /
            vol(i, j, k);
    rr = qyp(i, j, k, QRHO);
    rrnew = rr - flxrho;
    compo = rr * qyp(i, j, k, nqp) - compn;
    qp(i, j, k, nqp) = compo / rrnew + hdt * srcpass;

    rr = qym(i, j + 1, k, QRHO);
    rrnew = rr - flxrho;
    compo = rr * qym(i, j + 1, k, nqp) - compn;
    qm(i, j + 1, k, nqp) = compo / rrnew + hdt * srcpass;
  }

  pggp = qint(i + 1, j, k, GDPRES);
  pggm = qint(i, j, k, GDPRES);
  ugp = qint(i + 1, j, k, GDU);
  ugm = qint(i, j, k, GDU);

  dAup = a1(i + 1, j, k) * pggp * ugp - a1(i, j, k) * pggm * ugm;
  pav = 0.5 * (pggp + pggm);
  dAu = a1(i + 1, j, k) * ugp - a1(i, j, k) * ugm;

  // QP

  // Convert to conservative
  rrr = qyp(i, j, k, QRHO);
  rur = qyp(i, j, k, QU);
  rvr = qyp(i, j, k, QV);
  ekinr = 0.5 * rrr * (rur * rur + rvr * rvr);
  rur *= rrr;
  rvr *= rrr;

  rer = qyp(i, j, k, QREINT) + ekinr;
  // Add transverse predictor
  rrnewr = rrr - flxrho;
  runewr = rur - flxu;
  rvnewr = rvr - flxv;
  renewr = rer - flxe;

  // Convert back to primitive
  qp(i, j, k, QRHO) = rrnewr + hdt * srcr;
  qp(i, j, k, QU) = runewr / rrnewr + hdt * srcQ(i, j, k, QU);
  qp(i, j, k, QV) = rvnewr / rrnewr + hdt * srcQ(i, j, k, QV);
  rhoekinr = 0.5 * (runewr * runewr + rvnewr * rvnewr) / rrnewr;
  amrex::Real pnewr =
    qyp(i, j, k, QPRES) - hdt * (dAup + pav * dAu * (c - 1.)) / vol(i, j, k);
  pnewr += hdt * srcp;
  qp(i, j, k, QPRES) = amrex::max(pnewr, SMALL_PRES);
  qp(i, j, k, QREINT) = renewr - rhoekinr + hdt * srce;

  // QM

  // Conversion to Conservative
  rrl = qym(i, j + 1, k, QRHO);
  rul = qym(i, j + 1, k, QU);
  rvl = qym(i, j + 1, k, QV);
  ekinl = 0.5 * rrl * (rul * rul + rvl * rvl);
  rul *= rrl;
  rvl *= rrl;
  rel = qym(i, j + 1, k, QREINT) + ekinl;

  // Transverse fluxes
  rrnewl = rrl - flxrho;
  runewl = rul - flxu;
  rvnewl = rvl - flxv;
  renewl = rel - flxe;

  qm(i, j + 1, k, QRHO) = rrnewl + hdt * srcr;
  qm(i, j + 1, k, QU) = runewl / rrnewl + hdt * srcQ(i, j, k, QU);
  qm(i, j + 1, k, QV) = rvnewl / rrnewl + hdt * srcQ(i, j, k, QV);
  rhoekinl = 0.5 * (runewl * runewl + rvnewl * rvnewl) / rrnewl;

  qm(i, j + 1, k, QREINT) = renewl - rhoekinl + hdt * srce;
  amrex::Real pnewl = qym(i, j + 1, k, QPRES) -
                      hdt * (dAup + pav * dAu * (c - 1.)) / vol(i, j, k);
  qm(i, j + 1, k, QPRES) = amrex::max(pnewl + hdt * srcp, SMALL_PRES);
}

// Corrects the left and right Y-states with the X-Flux
AMREX_GPU_DEVICE
AMREX_FORCE_INLINE
void
pc_transy(
  const int i,
  const int j,
  const int k,
  amrex::Array4<amrex::Real> const& qm,
  amrex::Array4<amrex::Real> const& qp,
  amrex::Array4<const amrex::Real> const& qxm,
  amrex::Array4<const amrex::Real> const& qxp,
  amrex::Array4<const amrex::Real> const& flxy,
  amrex::Array4<const amrex::Real> const& srcQ,
  amrex::Array4<const amrex::Real> const& qa,
  amrex::Array4<const amrex::Real> const& qint,
  const amrex::Real hdt,
  const amrex::Real cdtdy)
{
  //  Local variables to avoid reading from global memory too much
  amrex::Real compn, rr, rrnew, compo, c;
  amrex::Real rrr, rur, rvr, ekinr, rer, rrnewr;
  amrex::Real runewr, rvnewr, renewr, rhoekinr;
  amrex::Real rrl, rul, rvl, ekinl, rel, rrnewl, runewl, rvnewl, renewl;
  amrex::Real rhoekinl, srcr, srce, srcp, srcpass;
  amrex::Real pggp, pggm, ugp, ugm;
  amrex::Real dup, pav, du;
  amrex::Real flxrho, flxu, flxv, flxe;

  flxrho = cdtdy * (flxy(i, j + 1, k, URHO) - flxy(i, j, k, URHO));
  flxu = cdtdy * (flxy(i, j + 1, k, UMX) - flxy(i, j, k, UMX));
  flxv = cdtdy * (flxy(i, j + 1, k, UMY) - flxy(i, j, k, UMY));
  flxe = cdtdy * (flxy(i, j + 1, k, UEDEN) - flxy(i, j, k, UEDEN));
  srcr = srcQ(i, j, k, QRHO);
  srce = srcQ(i, j, k, QREINT);
  srcp = srcQ(i, j, k, QPRES);
  c = qa(i, j, k, QGAMC);

  // Update passive variables
  for (int ipass = 0; ipass < NPASSIVE; ++ipass) {
    int n = indxmap::upass_map(ipass);
    int nqp = indxmap::qpass_map(ipass);
    srcpass = srcQ(i, j, k, nqp);
    compn = cdtdy * (flxy(i, j + 1, k, n) - flxy(i, j, k, n));
    rr = qxp(i, j, k, QRHO);
    rrnew = rr - flxrho;
    compo = rr * qxp(i, j, k, nqp) - compn;
    qp(i, j, k, nqp) = compo / rrnew + hdt * srcpass;

    rr = qxm(i + 1, j, k, QRHO);
    rrnew = rr - flxrho;
    compo = rr * qxm(i + 1, j, k, nqp) - compn;
    qm(i + 1, j, k, nqp) = compo / rrnew + hdt * srcpass;
  }

  // Update hydro vars
  pggp = qint(i, j + 1, k, GDPRES);
  pggm = qint(i, j, k, GDPRES);
  ugp = qint(i, j + 1, k, GDV);
  ugm = qint(i, j, k, GDV);

  dup = pggp * ugp - pggm * ugm;
  pav = 0.5 * (pggp + pggm);
  du = ugp - ugm;

  // QP

  // Convert to conservative
  rrr = qxp(i, j, k, QRHO);
  rur = qxp(i, j, k, QU);
  rvr = qxp(i, j, k, QV);
  ekinr = 0.5 * rrr * (rur * rur + rvr * rvr);
  rur *= rrr;
  rvr *= rrr;

  rer = qxp(i, j, k, QREINT) + ekinr;
  // Add transverse predictor
  rrnewr = rrr - flxrho;
  runewr = rur - flxu;
  rvnewr = rvr - flxv;
  renewr = rer - flxe;

  // Convert back to primitive
  qp(i, j, k, QRHO) = rrnewr + hdt * srcr;
  qp(i, j, k, QU) = runewr / rrnewr + hdt * srcQ(i, j, k, QU);
  qp(i, j, k, QV) = rvnewr / rrnewr + hdt * srcQ(i, j, k, QV);
  rhoekinr = 0.5 * (runewr * runewr + rvnewr * rvnewr) / rrnewr;
  amrex::Real pnewr = qxp(i, j, k, QPRES) - cdtdy * (dup + pav * du * (c - 1.));
  qp(i, j, k, QPRES) = amrex::max(pnewr + hdt * srcp, SMALL_PRES);
  qp(i, j, k, QREINT) = renewr - rhoekinr + hdt * srce;

  // QM

  // Conversion to Conservative
  rrl = qxm(i + 1, j, k, QRHO);
  rul = qxm(i + 1, j, k, QU);
  rvl = qxm(i + 1, j, k, QV);
  ekinl = 0.5 * rrl * (rul * rul + rvl * rvl);
  rul *= rrl;
  rvl *= rrl;
  rel = qxm(i + 1, j, k, QREINT) + ekinl;

  // Transverse fluxes
  rrnewl = rrl - flxrho;
  runewl = rul - flxu;
  rvnewl = rvl - flxv;
  renewl = rel - flxe;

  qm(i + 1, j, k, QRHO) = rrnewl + hdt * srcr;
  qm(i + 1, j, k, QU) = runewl / rrnewl + hdt * srcQ(i, j, k, QU);
  qm(i + 1, j, k, QV) = rvnewl / rrnewl + hdt * srcQ(i, j, k, QV);
  rhoekinl = 0.5 * (runewl * runewl + rvnewl * rvnewl) / rrnewl;

  amrex::Real pnewl =
    qxm(i + 1, j, k, QPRES) - cdtdy * (dup + pav * du * (c - 1.));
  qm(i + 1, j, k, QPRES) = amrex::max(pnewl + hdt * srcp, SMALL_PRES);
  qm(i + 1, j, k, QREINT) = renewl - rhoekinl + hdt * srce;
}

#endif
