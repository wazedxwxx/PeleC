  ! Compute the external sources for all the conservative equations.
  ! 
  ! This is called twice in the evolution:
  ! 
  ! First, for the predictor, it is called with (old, old) states.
  !
  ! This is also used in the first pass of the conservative update
  ! (adding dt * S there).
  !
  ! Next we correct the source terms in the conservative update to
  ! time-center them.  Here we call ext_src(old, new), and then
  ! in time_center_source_terms we subtract off 1/2 of the first S
  ! and add 1/2 of the new S.
  !
  ! Therefore, to get a properly time-centered source, generally
  ! speaking, you always want to use the "new" state here.  That
  ! will be the time n state in the first call and the n+1 in the
  ! second call.
  
module pc_ext_src_module

  implicit none

  private

  public :: pc_ext_src

contains

  subroutine pc_ext_src(lo,hi,&
                        old_state,os_lo,os_hi,&
                        new_state,ns_lo,ns_hi,&
                        src,src_lo,src_hi,problo,dx,time,dt) bind(C, name = "pc_ext_src")

    use amrex_constants_module, only: ZERO
    use meth_params_module, only : NVAR

    implicit none

    integer,          intent(in   ) :: lo(3),hi(3)
    integer,          intent(in   ) :: os_lo(3),os_hi(3)
    integer,          intent(in   ) :: ns_lo(3),ns_hi(3)
    integer,          intent(in   ) :: src_lo(3),src_hi(3)
    double precision, intent(in   ) :: old_state(os_lo(1):os_hi(1),os_lo(2):os_hi(2),os_lo(3):os_hi(3),NVAR)
    double precision, intent(in   ) :: new_state(ns_lo(1):ns_hi(1),ns_lo(2):ns_hi(2),ns_lo(3):ns_hi(3),NVAR)
    double precision, intent(inout) :: src(src_lo(1):src_hi(1),src_lo(2):src_hi(2),src_lo(3):src_hi(3),NVAR)
    double precision, intent(in   ) :: problo(3),dx(3),time,dt

    ! lo and hi specify work region
    src(lo(1):hi(1),lo(2):hi(2),lo(3):hi(3),:) = ZERO ! Fill work region only

  end subroutine pc_ext_src

end module pc_ext_src_module
