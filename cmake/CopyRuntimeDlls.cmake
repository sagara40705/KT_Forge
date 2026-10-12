foreach(dll IN LISTS dlls)
    file(COPY "${dll}" DESTINATION "${destination}")
endforeach()
