string(TIMESTAMP BUILD_DATE "%Y-%m-%d")
LIST(APPEND FREEDESKTOP_RELEASE_INFO
    "<release version=\"git-continuous\" date=\"${BUILD_DATE}\" />"
)

configure_file(
    ${CMAKE_SOURCE_DIR}/data/org.vita3k.vita3k.metainfo.xml.in
    ${CMAKE_SOURCE_DIR}/data/org.vita3k.vita3k.metainfo.xml
)