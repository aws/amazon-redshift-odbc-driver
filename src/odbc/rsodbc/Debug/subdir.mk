################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../dllmain.c \
../rslibpq.c \
../rstrace.c \
../rsunicode.c \
../rsutil.c \
../win_port.c 

CPP_SRCS += \
../rsini.cpp \
../rstransaction.cpp \
../rsparameter.cpp \
../rserror.cpp \
../rsdesc.cpp \
../rsoptions.cpp \
../rsdrvinfo.cpp \
../rscatalog.cpp \
../rsprepare.cpp \
../rsexecute.cpp \
../rsconnect.cpp \
../rsresult.cpp

OBJS += \
./dllmain.o \
./rslibpq.o \
./rstrace.o \
./rsunicode.o \
./rsutil.o \
./win_port.o \
./rsini.o \
./rstransaction.o \
./rsparameter.o \
./rserror.o \
./rsdesc.o \
./rsoptions.o \
./rsdrvinfo.o \
./rscatalog.o \
./rsprepare.o \
./rsexecute.o \
./rsconnect.o \
./rsresult.o 

C_DEPS += \
./dllmain.d \
./rslibpq.d \
./rstrace.d \
./rsunicode.d \
./rsutil.d \
./win_port.d 


 CPP_DEPS += \
  ./rsini.d \
  ./rstransaction.d \
  ./rsparameter.d \
  ./rserror.d \
  ./rsdesc.d \
  ./rsoptions.d \
  ./rsdrvinfo.d \
  ./rscatalog.d \
  ./rsprepare.d \
  ./rsexecute.d \
  ./rsconnect.d \
  ./rsresult.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	g++ -DLINUX -DUSE_SSL -DBUILD_REAL_64_BIT_MODE -DODBCVER=0x0352 -I../../../pgclient/src/interfaces/libpq -I../../../pgclient/src/include -O0 -g3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

%.o : ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -DLINUX -DUSE_SSL -DODBCVER=0x0352 -DBUILD_REAL_64_BIT_MODE -I../../../pgclient/src/interfaces/libpq -I../../../pgclient/src/include -I/usr/local/ssl/include -O0 -g3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


