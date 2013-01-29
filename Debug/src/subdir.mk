################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/base.c \
../src/sample.c \
../src/scheduler.c \
../src/state_printer.c \
../src/test.c \
../src/z502.c 

OBJS += \
./src/base.o \
./src/sample.o \
./src/scheduler.o \
./src/state_printer.o \
./src/test.o \
./src/z502.o 

C_DEPS += \
./src/base.d \
./src/sample.d \
./src/scheduler.d \
./src/state_printer.d \
./src/test.d \
./src/z502.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -pthread -lm -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


