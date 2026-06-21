################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../codec2/src/HRA_112_112.c \
../codec2/src/HRA_56_56.c \
../codec2/src/HRAa_1536_512.c \
../codec2/src/HRAb_396_504.c \
../codec2/src/H_1024_2048_4f.c \
../codec2/src/codec2_fifo.c \
../codec2/src/filter.c \
../codec2/src/freedv_700.c \
../codec2/src/freedv_api.c \
../codec2/src/freedv_data_channel.c \
../codec2/src/gp_interleaver.c \
../codec2/src/interldpc.c \
../codec2/src/kiss_fft.c \
../codec2/src/kiss_fftr.c \
../codec2/src/modem_stats.c \
../codec2/src/mpdecode_core.c \
../codec2/src/ofdm.c \
../codec2/src/ofdm_mode.c \
../codec2/src/pack.c \
../codec2/src/phi0.c \
../codec2/src/varicode.c 

OBJS += \
./codec2/src/HRA_112_112.o \
./codec2/src/HRA_56_56.o \
./codec2/src/HRAa_1536_512.o \
./codec2/src/HRAb_396_504.o \
./codec2/src/H_1024_2048_4f.o \
./codec2/src/codec2_fifo.o \
./codec2/src/filter.o \
./codec2/src/freedv_700.o \
./codec2/src/freedv_api.o \
./codec2/src/freedv_data_channel.o \
./codec2/src/gp_interleaver.o \
./codec2/src/interldpc.o \
./codec2/src/kiss_fft.o \
./codec2/src/kiss_fftr.o \
./codec2/src/modem_stats.o \
./codec2/src/mpdecode_core.o \
./codec2/src/ofdm.o \
./codec2/src/ofdm_mode.o \
./codec2/src/pack.o \
./codec2/src/phi0.o \
./codec2/src/varicode.o 

C_DEPS += \
./codec2/src/HRA_112_112.d \
./codec2/src/HRA_56_56.d \
./codec2/src/HRAa_1536_512.d \
./codec2/src/HRAb_396_504.d \
./codec2/src/H_1024_2048_4f.d \
./codec2/src/codec2_fifo.d \
./codec2/src/filter.d \
./codec2/src/freedv_700.d \
./codec2/src/freedv_api.d \
./codec2/src/freedv_data_channel.d \
./codec2/src/gp_interleaver.d \
./codec2/src/interldpc.d \
./codec2/src/kiss_fft.d \
./codec2/src/kiss_fftr.d \
./codec2/src/modem_stats.d \
./codec2/src/mpdecode_core.d \
./codec2/src/ofdm.d \
./codec2/src/ofdm_mode.d \
./codec2/src/pack.d \
./codec2/src/phi0.d \
./codec2/src/varicode.d 


# Each subdirectory must supply rules for building sources it contributes
codec2/src/%.o codec2/src/%.su codec2/src/%.cyclo: ../codec2/src/%.c codec2/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DCORE_CM4 -DUSE_HAL_DRIVER -DSTM32H755xx -DUSE_PWR_DIRECT_SMPS_SUPPLY -c -I../Core/Inc -I"C:/Users/DANIL/STM32CubeIDE/workspace_2.1.0/tstm32h55_main/CM4/codec2/src" -I"C:/Users/DANIL/STM32CubeIDE/workspace_2.1.0/tstm32h55_main/inc" -I../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../Drivers/CMSIS/Include -Og -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-codec2-2f-src

clean-codec2-2f-src:
	-$(RM) ./codec2/src/HRA_112_112.cyclo ./codec2/src/HRA_112_112.d ./codec2/src/HRA_112_112.o ./codec2/src/HRA_112_112.su ./codec2/src/HRA_56_56.cyclo ./codec2/src/HRA_56_56.d ./codec2/src/HRA_56_56.o ./codec2/src/HRA_56_56.su ./codec2/src/HRAa_1536_512.cyclo ./codec2/src/HRAa_1536_512.d ./codec2/src/HRAa_1536_512.o ./codec2/src/HRAa_1536_512.su ./codec2/src/HRAb_396_504.cyclo ./codec2/src/HRAb_396_504.d ./codec2/src/HRAb_396_504.o ./codec2/src/HRAb_396_504.su ./codec2/src/H_1024_2048_4f.cyclo ./codec2/src/H_1024_2048_4f.d ./codec2/src/H_1024_2048_4f.o ./codec2/src/H_1024_2048_4f.su ./codec2/src/codec2_fifo.cyclo ./codec2/src/codec2_fifo.d ./codec2/src/codec2_fifo.o ./codec2/src/codec2_fifo.su ./codec2/src/filter.cyclo ./codec2/src/filter.d ./codec2/src/filter.o ./codec2/src/filter.su ./codec2/src/freedv_700.cyclo ./codec2/src/freedv_700.d ./codec2/src/freedv_700.o ./codec2/src/freedv_700.su ./codec2/src/freedv_api.cyclo ./codec2/src/freedv_api.d ./codec2/src/freedv_api.o ./codec2/src/freedv_api.su ./codec2/src/freedv_data_channel.cyclo ./codec2/src/freedv_data_channel.d ./codec2/src/freedv_data_channel.o ./codec2/src/freedv_data_channel.su ./codec2/src/gp_interleaver.cyclo ./codec2/src/gp_interleaver.d ./codec2/src/gp_interleaver.o ./codec2/src/gp_interleaver.su ./codec2/src/interldpc.cyclo ./codec2/src/interldpc.d ./codec2/src/interldpc.o ./codec2/src/interldpc.su ./codec2/src/kiss_fft.cyclo ./codec2/src/kiss_fft.d ./codec2/src/kiss_fft.o ./codec2/src/kiss_fft.su ./codec2/src/kiss_fftr.cyclo ./codec2/src/kiss_fftr.d ./codec2/src/kiss_fftr.o ./codec2/src/kiss_fftr.su ./codec2/src/modem_stats.cyclo ./codec2/src/modem_stats.d ./codec2/src/modem_stats.o ./codec2/src/modem_stats.su ./codec2/src/mpdecode_core.cyclo ./codec2/src/mpdecode_core.d ./codec2/src/mpdecode_core.o ./codec2/src/mpdecode_core.su ./codec2/src/ofdm.cyclo ./codec2/src/ofdm.d ./codec2/src/ofdm.o ./codec2/src/ofdm.su ./codec2/src/ofdm_mode.cyclo ./codec2/src/ofdm_mode.d ./codec2/src/ofdm_mode.o ./codec2/src/ofdm_mode.su ./codec2/src/pack.cyclo ./codec2/src/pack.d ./codec2/src/pack.o ./codec2/src/pack.su ./codec2/src/phi0.cyclo ./codec2/src/phi0.d ./codec2/src/phi0.o ./codec2/src/phi0.su ./codec2/src/varicode.cyclo ./codec2/src/varicode.d ./codec2/src/varicode.o ./codec2/src/varicode.su

.PHONY: clean-codec2-2f-src

