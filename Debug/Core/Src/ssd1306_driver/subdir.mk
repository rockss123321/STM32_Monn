################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/ssd1306_driver/ssd1306.c \
../Core/Src/ssd1306_driver/ssd1306_fonts.c \
../Core/Src/ssd1306_driver/ssd1306_tests.c 

OBJS += \
./Core/Src/ssd1306_driver/ssd1306.o \
./Core/Src/ssd1306_driver/ssd1306_fonts.o \
./Core/Src/ssd1306_driver/ssd1306_tests.o 

C_DEPS += \
./Core/Src/ssd1306_driver/ssd1306.d \
./Core/Src/ssd1306_driver/ssd1306_fonts.d \
./Core/Src/ssd1306_driver/ssd1306_tests.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/ssd1306_driver/%.o Core/Src/ssd1306_driver/%.su Core/Src/ssd1306_driver/%.cyclo: ../Core/Src/ssd1306_driver/%.c Core/Src/ssd1306_driver/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F207xx -c -I../Core/Inc -I../Drivers/STM32F2xx_HAL_Driver/Inc -I../Drivers/STM32F2xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F2xx/Include -I../Drivers/CMSIS/Include -I../LWIP/App -I../LWIP/Target -I../Middlewares/Third_Party/LwIP/src/include -I../Middlewares/Third_Party/LwIP/system -I../Middlewares/Third_Party/LwIP/src/include/netif/ppp -I../Middlewares/Third_Party/LwIP/src/include/lwip -I../Middlewares/Third_Party/LwIP/src/include/lwip/apps -I../Middlewares/Third_Party/LwIP/src/include/lwip/priv -I../Middlewares/Third_Party/LwIP/src/include/lwip/prot -I../Middlewares/Third_Party/LwIP/src/include/netif -I../Middlewares/Third_Party/LwIP/src/include/posix -I../Middlewares/Third_Party/LwIP/src/include/posix/sys -I../Middlewares/Third_Party/LwIP/system/arch -I../Middlewares/Third_Party/LwIP/src/apps/httpd -I../Middlewares/Third_Party/LwIP/src/apps/snmp -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-Src-2f-ssd1306_driver

clean-Core-2f-Src-2f-ssd1306_driver:
	-$(RM) ./Core/Src/ssd1306_driver/ssd1306.cyclo ./Core/Src/ssd1306_driver/ssd1306.d ./Core/Src/ssd1306_driver/ssd1306.o ./Core/Src/ssd1306_driver/ssd1306.su ./Core/Src/ssd1306_driver/ssd1306_fonts.cyclo ./Core/Src/ssd1306_driver/ssd1306_fonts.d ./Core/Src/ssd1306_driver/ssd1306_fonts.o ./Core/Src/ssd1306_driver/ssd1306_fonts.su ./Core/Src/ssd1306_driver/ssd1306_tests.cyclo ./Core/Src/ssd1306_driver/ssd1306_tests.d ./Core/Src/ssd1306_driver/ssd1306_tests.o ./Core/Src/ssd1306_driver/ssd1306_tests.su

.PHONY: clean-Core-2f-Src-2f-ssd1306_driver

