################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
-include $(CPP_DEPS)
endif
endif

LIBS_PATH := -L${ROOT_DIR}/src/logging -L../../../pgclient/src/interfaces/libpq/Release -L../../../pgclient/src/port/Release -L$(OPENSSL_LIB_DIR) -L$(AWS_SDK_LIB_DIR) -L$(CURL_LIB_DIR)

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: librsodbc64.so

# Tool invocations
librsodbc64.so: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	g++ -std=gnu++17 -Wl,--version-script=../exports.list -Wl,--no-undefined -shared -o "librsodbc64.so" $(OBJS) $(USER_OBJS) $(LIBS_PATH) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(OBJS)$(C_DEPS)$(CPP_DEPS)$(LIBRARIES) librsodbc64.so
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets
