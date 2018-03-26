#
# Source files
#
# Here is where you can add the name of your local Transform classes.
#


PROJECT(zxhImageProcessings)
INCLUDE_DIRECTORIES(${zxhImageProcessings_SOURCE_DIR})


FILE(GLOB zxhImages_files "*.*")

SET (zxhImages_SRCS
${zxhImages_files}
)
 
# Create the zxhImages C++ library
ADD_LIBRARY (zxhImages ${zxhImages_SRCS})


TARGET_LINK_LIBRARIES(zxhImages zxhCommon niftiio znz ${NIFTI_NIFTILIB_NAME})

INSTALL_TARGETS(/lib/zxh zxhImages)

INSTALL_FILES(/include/zxh .h ${zxhImages_SRCS})
