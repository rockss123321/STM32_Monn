################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/oled/oled_abpage.c \
../Core/Src/oled/oled_display.c \
../Core/Src/oled/oled_netinfo.c \
../Core/Src/oled/oled_settings.c 

OBJS += \
./Core/Src/oled/oled_abpage.o \
./Core/Src/oled/oled_display.o \
./Core/Src/oled/oled_netinfo.o \
./Core/Src/oled/oled_settings.o 

C_DEPS += \
./Core/Src/oled/oled_abpage.d \
./Core/Src/oled/oled_display.d \
./Core/Src/oled/oled_netinfo.d \
./Core/Src/oled/oled_settings.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/oled/%.o Core/Src/oled/%.su Core/Src/oled/%.cyclo: ../Core/Src/oled/%.c Core/Src/oled/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F207xx -c -I../Core/Inc -I../Drivers/STM32F2xx_HAL_Driver/Inc -I../Drivers/STM32F2xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F2xx/Include -I../Drivers/CMSIS/Include -I../LWIP/App -I../LWIP/Target -I../Middlewares/Third_Party/LwIP/src/include -I../Middlewares/Third_Party/LwIP/system -I../Middlewares/Third_Party/LwIP/src/include/netif/ppp -I../Middlewares/Third_Party/LwIP/src/include/lwip -I../Middlewares/Third_Party/LwIP/src/include/lwip/apps -I../Middlewares/Third_Party/LwIP/src/include/lwip/priv -I../Middlewares/Third_Party/LwIP/src/include/lwip/prot -I../Middlewares/Third_Party/LwIP/src/include/netif -I../Middlewares/Third_Party/LwIP/src/include/posix -I../Middlewares/Third_Party/LwIP/src/include/posix/sys -I../Middlewares/Third_Party/LwIP/system/arch -I../Middlewares/Third_Party/LwIP/src/apps/httpd -I../Middlewares/Third_Party/LwIP/src/apps/snmp -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-Src-2f-oled

clean-Core-2f-Src-2f-oled:
	-$(RM) ./Core/Src/oled/oled_abpage.cyclo ./Core/Src/oled/oled_abpage.d ./Core/Src/oled/oled_abpage.o ./Core/Src/oled/oled_abpage.su ./Core/Src/oled/oled_display.cyclo ./Core/Src/oled/oled_display.d ./Core/Src/oled/oled_display.o ./Core/Src/oled/oled_display.su ./Core/Src/oled/oled_netinfo.cyclo ./Core/Src/oled/oled_netinfo.d ./Core/Src/oled/oled_netinfo.o ./Core/Src/oled/oled_netinfo.su ./Core/Src/oled/oled_settings.cyclo ./Core/Src/oled/oled_settings.d ./Core/Src/oled/oled_settings.o ./Core/Src/oled/oled_settings.su

.PHONY: clean-Core-2f-Src-2f-oled

