NAME = vulkan
CC = clang++
CFLAGS = -stdlib=libstdc++ -g3 -gdwarf-4 -std=c++26 #-Werror -Wextra -Wall

#__directory__
SRC_DIR = src/
OBJ_DIR = obj/
INC_DIR = inc/

#__src__
SRC_FILES = \
	util.cpp\
	main.cpp\
	vma.cpp\
	App.cpp

SRC = $(addprefix $(SRC_DIR), $(SRC_FILES))

#__obj__
OBJ_FILES = $(SRC_FILES:.cpp=.o)
OBJ = $(addprefix $(OBJ_DIR), $(OBJ_FILES))

#__dependence__
DEP = $(OBJ:.o=.d)

#__include__
INC = -I $(INC_DIR)

#__lib__
#sdl builded in 3.4.16
SDL = /home/edarnand/Downloads/sdl_3.4.16/
SDL_LIB = $(SDL)build/libSDL3.so
INC += -I $(SDL)include

VULKAN = /home/edarnand/sgoinfre/vulkan/1.4.357.1/x86_64/
VULKAN_LIB = $(VULKAN)lib/VulkanLoader/lib/libvulkan.so.1.4.357
SHADERC_LIB = $(VULKAN)lib/libshaderc_combined.a
VOLK_LIB = $(VULKAN)lib/libvolk.a
INC += -I $(VULKAN)include

GLM = /home/edarnand/Downloads/glm/
GLM_LIB = $(GLM)build/glm/libglm.a
INC += -I $(GLM)build_share/include

LIB = $(SDL_LIB) $(GLM_LIB) $(VOLK_LIB) $(VULKAN_LIB) $(SHADERC_LIB)

#__rules__
all: $(NAME)

$(NAME): $(OBJ)
	$(CC) -ldl $(CFLAGS) -lm $(OBJ) -o $(NAME) $(LIB)

$(OBJ_DIR)%.o:$(SRC_DIR)%.cpp Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP  $(INC) -c $< -o $@

#__cleaning__
clean:
	rm -rf $(OBJ_DIR)

fclean:
	rm -rf $(OBJ_DIR) $(NAME)

re:
	$(MAKE) fclean
	$(MAKE) all

-include $(DEP)

.PHONY: all clean fclean re
