USE_EB = FALSE
PRECISION  = DOUBLE
PROFILE    = FALSE
DEBUG      = FALSE
DIM        = 3
COMP	   = gcc
USE_MPI    = FALSE
USE_REACT  = TRUE

###### CVODE
USE_SUNDIALS_PP = FALSE
USE_KLU_PP = FALSE

#HYP_TYPE   = MOL

# This sets the EOS directory in $(PELE_PHYSICS_HOME)/Eos
#Eos_dir     := Soave-Redlich-Kwong
Eos_dir     := Fuego

# This sets the model directory in $(PELE_PHYSICS_HOME)/Support/Fuego/Mechanism/Models
#Chemistry_Model := Davis
Chemistry_Model := LiDryer

# This sets the transport directory in $(PELE_PHYSICS_HOME)/Transport
Transport_dir := EGLib
#Transport_dir := Simple

Bpack   := ./Make.package
Blocs   := .

# define the location of the PELE top directory
PELEC_HOME    := ../../..
include $(PELEC_HOME)/Exec/Make.PeleC
