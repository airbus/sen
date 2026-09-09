# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

find_program(mkdocs_exec NAMES mkdocs)

# ===================================================================================================================
# Mkdocs
# ===================================================================================================================
if(mkdocs_exec)

  # create snippets dir if it does not exist
  set(snippets_dir ${PROJECT_SOURCE_DIR}/docs/snippets)
  execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${snippets_dir})

  # create the name of the script that we will use to generate snippets
  if(WIN32)
    set(snippets_gen_script "${CMAKE_CURRENT_BINARY_DIR}/generate_snippets_$<CONFIG>.bat")
    set(sen_exec "sen")
  else()
    set(snippets_gen_script "${CMAKE_CURRENT_BINARY_DIR}/generate_snippets_$<CONFIG>.sh")
    set(sen_exec "./sen")
  endif()

  set(fom_base_dir ${PROJECT_SOURCE_DIR}/examples/packages/hla_fom)

  # write the script
  file(
    GENERATE
    OUTPUT ${snippets_gen_script}
    CONTENT
      "cd $<TARGET_FILE_DIR:cli_sen>\n\
${sen_exec} --help > ${snippets_dir}/sen.sh\n\
\n\
${sen_exec} run --help > ${snippets_dir}/sen_run.sh\n\
${sen_exec} file-to-array --help > ${snippets_dir}/sen_file_to_array.sh\n\
${sen_exec} generate --help > ${snippets_dir}/sen_generate.sh\n\
\n\
${sen_exec} generate cpp --help > ${snippets_dir}/sen_generate_cpp.sh\n\
${sen_exec} generate cpp stl --help > ${snippets_dir}/sen_generate_cpp_stl.sh\n\
${sen_exec} generate cpp fom --help > ${snippets_dir}/sen_generate_cpp_fom.sh\n\
${sen_exec} generate cpp exports --help > ${snippets_dir}/sen_generate_cpp_exports.sh\n\
\n\
${sen_exec} generate uml --help > ${snippets_dir}/sen_generate_uml.sh\n\
${sen_exec} generate uml stl --help > ${snippets_dir}/sen_generate_uml_stl.sh\n\
${sen_exec} generate uml fom --help > ${snippets_dir}/sen_generate_uml_fom.sh\n\
\n\
${sen_exec} generate mkdocs --help > ${snippets_dir}/sen_generate_mkdocs.sh\n\
${sen_exec} generate mkdocs stl --help > ${snippets_dir}/sen_generate_mkdocs_stl.sh\n\
${sen_exec} generate mkdocs fom --help > ${snippets_dir}/sen_generate_mkdocs_fom.sh\n\
\n\
${sen_exec} generate html --help > ${snippets_dir}/sen_generate_html.sh\n\
${sen_exec} generate html stl --help > ${snippets_dir}/sen_generate_html_stl.sh\n\
${sen_exec} generate html fom --help > ${snippets_dir}/sen_generate_html_fom.sh\n\
\n\
${sen_exec} generate typst --help > ${snippets_dir}/sen_generate_typst.sh\n\
${sen_exec} generate typst stl --help > ${snippets_dir}/sen_generate_typst_stl.sh\n\
${sen_exec} generate typst fom --help > ${snippets_dir}/sen_generate_typst_fom.sh\n\
\n\
${sen_exec} generate json package stl --help > ${snippets_dir}/sen_generate_json_package_stl.sh\n\
${sen_exec} generate json package fom --help > ${snippets_dir}/sen_generate_json_package_fom.sh\n\
\n\
${sen_exec} generate json component stl --help > ${snippets_dir}/sen_generate_json_component_stl.sh\n\
${sen_exec} generate json component fom --help > ${snippets_dir}/sen_generate_json_component_fom.sh\n\
\n\
${sen_exec} generate json schema --help > ${snippets_dir}/sen_generate_json_schema.sh\n\
\n\
${sen_exec} generate py --help > ${snippets_dir}/sen_generate_py.sh\n\
${sen_exec} generate py stl --help > ${snippets_dir}/sen_generate_py_stl.sh\n\
${sen_exec} generate py fom --help > ${snippets_dir}/sen_generate_py_fom.sh\n\
\n\
${sen_exec} generate ts --help > ${snippets_dir}/sen_generate_ts.sh\n\
${sen_exec} generate ts stl --help > ${snippets_dir}/sen_generate_ts_stl.sh\n\
\n\
${sen_exec} archive --help > ${snippets_dir}/sen_archive.sh\n\
${sen_exec} archive indexed --help > ${snippets_dir}/sen_archive_indexed.sh\n\
${sen_exec} archive info --help > ${snippets_dir}/sen_archive_info.sh\n\
\n\
${sen_exec} package --help > ${snippets_dir}/sen_package.sh\n\
${sen_exec} package init --help > ${snippets_dir}/sen_package_init.sh\n\
\n\
\"${CMAKE_COMMAND}\" -E rm -rf ${snippets_dir}/my_package\n\
${sen_exec} package init ${snippets_dir}/my_package --class MyClass\n\
${sen_exec} package init-component --help > ${snippets_dir}/sen_package_init-component.sh\n\
\n\
exit 0"
      FILE_PERMISSIONS
      OWNER_READ
      OWNER_WRITE
      OWNER_EXECUTE
      GROUP_READ
      GROUP_WRITE
      GROUP_EXECUTE
      WORLD_READ
      WORLD_WRITE
      WORLD_EXECUTE
  )

  sen_generate_html(
    TARGET fom_reference
    OUT ${snippets_dir}/fom
    TITLE "RPR, NETN and Link 16"
    HLA_FOM_DIRS ${fom_base_dir}/rpr ${fom_base_dir}/netn ${fom_base_dir}/link16
  )

  # Two documents, because they show different things: a FOM is one deep class tree in two
  # packages, and Sen's own API is fifteen packages three levels deep. PDF, so the pages can
  # link to something a reader opens rather than to source they would have to compile.
  set(typst_pages_dir ${PROJECT_SOURCE_DIR}/docs/typst)

  sen_generate_typst(
    TARGET
    fom_document
    OUT
    ${snippets_dir}/fom-document
    TITLE
    "RPR, NETN and Link 16"
    HLA_FOM_DIRS
    ${fom_base_dir}/rpr
    ${fom_base_dir}/netn
    ${fom_base_dir}/link16
    FRONT_MATTER
    ${typst_pages_dir}/front-matter.typ
    BEFORE_REFERENCE
    ${typst_pages_dir}/about-this-example.typ
    PDF
  )

  file(
    GLOB
    _sen_api_stl
    CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/libs/kernel/stl/sen/kernel/*.stl
    ${PROJECT_SOURCE_DIR}/libs/db/stl/sen/db/*.stl
    ${PROJECT_SOURCE_DIR}/components/*/stl/*.stl
    ${PROJECT_SOURCE_DIR}/components/*/stl/sen/components/*/*.stl
  )

  # Each component roots its own imports, so one base path cannot resolve them all.
  file(
    GLOB _sen_api_imports
    LIST_DIRECTORIES true
    ${PROJECT_SOURCE_DIR}/components/* ${PROJECT_SOURCE_DIR}/components/*/stl
  )

  sen_generate_typst(
    TARGET
    api_document
    OUT
    ${snippets_dir}/api-document
    TITLE
    "The Sen API"
    STL_FILES
    ${_sen_api_stl}
    BASE_PATH
    ${PROJECT_SOURCE_DIR}/libs/kernel
    IMPORT_PATHS
    ${PROJECT_SOURCE_DIR}/libs/db
    ${_sen_api_imports}
    FRONT_MATTER
    ${typst_pages_dir}/front-matter.typ
    BEFORE_REFERENCE
    ${typst_pages_dir}/about-this-example.typ
    PDF
  )

  sen_generate_uml(
    TARGET fom_uml
    OUT ${snippets_dir}/fom.plantuml
    HLA_FOM_DIRS ${fom_base_dir}/rpr ${fom_base_dir}/netn ${fom_base_dir}/link16
    CLASSES_ONLY
  )

  # The example used to be one markdown page. snippets/ is gitignored and persists, so a tree
  # that built before the change would keep serving the old one from the site.
  file(REMOVE ${snippets_dir}/fom.md)

  set(snippets
      ${snippets_dir}/sen.sh
      ${snippets_dir}/LICENSE.txt
      ${snippets_dir}/sen_run.sh
      ${snippets_dir}/sen_file_to_array.sh
      ${snippets_dir}/sen_generate.sh
      ${snippets_dir}/sen_generate_cpp.sh
      ${snippets_dir}/sen_generate_cpp_stl.sh
      ${snippets_dir}/sen_generate_cpp_fom.sh
      ${snippets_dir}/sen_generate_cpp_exports.sh
      ${snippets_dir}/sen_generate_uml.sh
      ${snippets_dir}/sen_generate_uml_stl.sh
      ${snippets_dir}/sen_generate_uml_fom.sh
      ${snippets_dir}/sen_generate_mkdocs.sh
      ${snippets_dir}/sen_generate_mkdocs_stl.sh
      ${snippets_dir}/sen_generate_mkdocs_fom.sh
      ${snippets_dir}/sen_generate_html.sh
      ${snippets_dir}/sen_generate_html_stl.sh
      ${snippets_dir}/sen_generate_html_fom.sh
      ${snippets_dir}/sen_generate_typst.sh
      ${snippets_dir}/sen_generate_typst_stl.sh
      ${snippets_dir}/sen_generate_typst_fom.sh
      ${snippets_dir}/sen_generate_json_package_stl.sh
      ${snippets_dir}/sen_generate_json_package_fom.sh
      ${snippets_dir}/sen_generate_json_component_stl.sh
      ${snippets_dir}/sen_generate_json_component_fom.sh
      ${snippets_dir}/sen_generate_json_schema.sh
      ${snippets_dir}/sen_generate_py.sh
      ${snippets_dir}/sen_generate_py_stl.sh
      ${snippets_dir}/sen_generate_py_fom.sh
      ${snippets_dir}/sen_generate_ts.sh
      ${snippets_dir}/sen_generate_ts_stl.sh
      ${snippets_dir}/sen_archive.sh
      ${snippets_dir}/sen_archive_indexed.sh
      ${snippets_dir}/sen_archive_info.sh
      ${snippets_dir}/sen_package.sh
      ${snippets_dir}/sen_package_init.sh
      ${snippets_dir}/sen_package_init-component.sh
      # A real package, generated by the tool itself, so the getting-started page
      # shows what a reader actually gets rather than a copy that drifts.
      ${snippets_dir}/my_package/CMakeLists.txt
      ${snippets_dir}/my_package/config.yaml
      ${snippets_dir}/my_package/src/my_class.h
      ${snippets_dir}/my_package/src/my_class.cpp
      ${snippets_dir}/my_package/stl/my_package/my_class.stl
      ${snippets_dir}/my_package/stl/my_package/basic_types.stl
  )

  # Without DEPENDS the outputs were never stale, so the published help text
  # outlived the tools it documents.
  if(WIN32)
    add_custom_command(
      OUTPUT ${snippets}
      COMMAND ${snippets_gen_script}
      COMMENT "generating snippets"
      DEPENDS ${snippets_gen_script}
              cli_sen
              cli_gen
              cli_run
              cli_package
              cli_archive
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
  else()
    add_custom_command(
      OUTPUT ${snippets}
      COMMAND sh ${snippets_gen_script}
      COMMENT "generating snippets"
      DEPENDS ${snippets_gen_script}
              cli_sen
              cli_gen
              cli_run
              cli_package
              cli_archive
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
  endif()

  add_custom_target(snippets_target DEPENDS ${snippets})
  set_target_properties(snippets_target PROPERTIES FOLDER ".cmake")

  add_dependencies(
    snippets_target
    fom_uml
    fom_reference
    fom_document
    api_document
    cli_sen
    cli_gen
    cli_run
    cli_package
    cli_archive
  )

  file(
    GLOB_RECURSE doc_sources
    LIST_DIRECTORIES false
    *.md
    *.gif
    *.png
    *.jpg
    *.txt
  )

  # when deploying docs automatically we can set the site-dir to a different path to ease the update process
  if(NOT DEFINED ENV{MKDOCS_SITE_DIR})
    set(ENV{MKDOCS_SITE_DIR} "${CMAKE_CURRENT_BINARY_DIR}/site")
  endif()

  # ===================================================================================================================
  # Build the licenses documentation
  # ===================================================================================================================

  # Written at build time, not while configuring. conanfile.py copies the C++
  # licences during `conan install`, but the npm ones are baked by a build
  # target, so a page written at configure time can never hold them.
  find_package(Python3 REQUIRED COMPONENTS Interpreter)

  set(_lic_page ${PROJECT_SOURCE_DIR}/docs/licenses/licensed_code_in_sen.md)
  set(_lic_script ${PROJECT_SOURCE_DIR}/tools/generate_license_page.py)
  get_property(_lic_targets GLOBAL PROPERTY SEN_LICENSE_TARGETS)

  add_custom_command(
    OUTPUT ${_lic_page}
    COMMAND ${Python3_EXECUTABLE} ${_lic_script} --licenses-dir ${CMAKE_BINARY_DIR}/foss_licenses --output
            ${_lic_page}
    DEPENDS ${_lic_script} ${_lic_targets}
    COMMENT "generating the third-party licence page"
    VERBATIM
  )

  # ===================================================================================================================
  # Generate the mkdocs content
  # ===================================================================================================================

  # Pure copies: source files the site serves as-is. No binary produces any of
  # them, which is what lets docs_preview below run with no build at all.
  add_custom_target(
    docs_content
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/LICENSE.txt ${snippets_dir}/LICENSE.txt
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${PROJECT_SOURCE_DIR}/examples
            ${snippets_dir}/examples
    COMMAND
      ${CMAKE_COMMAND} -E copy
      ${PROJECT_SOURCE_DIR}/components/recorder/stl/sen/components/recorder/configuration.stl
      ${snippets_dir}/recorder_config.stl
    COMMAND
      ${CMAKE_COMMAND} -E copy
      ${PROJECT_SOURCE_DIR}/components/recorder/stl/sen/components/recorder/recorder.stl
      ${snippets_dir}/recorder.stl
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/components/logmaster/stl/logmaster.stl
            ${snippets_dir}/logmaster.stl
    COMMAND
      ${CMAKE_COMMAND} -E copy
      ${PROJECT_SOURCE_DIR}/components/replayer/stl/sen/components/replayer/configuration.stl
      ${snippets_dir}/replayer_config.stl
    COMMAND
      ${CMAKE_COMMAND} -E copy
      ${PROJECT_SOURCE_DIR}/components/replayer/stl/sen/components/replayer/replayer.stl
      ${snippets_dir}/replayer.stl
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/components/ether/stl/configuration.stl
            ${snippets_dir}/ether_config.stl
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/components/influx/stl/configuration.stl
            ${snippets_dir}/influx_config.stl
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/components/py/stl/python_interpreter.stl
            ${snippets_dir}/python_interpreter.stl
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/components/shell/stl/shell.stl
            ${snippets_dir}/shell.stl
    COMMENT "staging the documentation content that needs no build"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    VERBATIM
  )
  set_target_properties(docs_content PROPERTIES FOLDER "docs")

  # The site in seconds, without Conan, a compiler, doxygen or plantuml. What
  # those would have produced is stood in for by placeholders, so a typo fix
  # does not cost a build. The two doxygen pages are named because five library
  # pages link to them and strict mode fails a link with no target.
  # Never publish this: the content is not real.
  add_custom_target(
    docs_preview
    COMMAND
      ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/stage_doc_placeholders.py --docs
      ${PROJECT_SOURCE_DIR}/docs ${snippets} ${snippets_dir}/fom.svg ${_lic_page}
      ${PROJECT_SOURCE_DIR}/docs/doxygen_gen/html/index.html
      ${PROJECT_SOURCE_DIR}/docs/doxygen_gen/html/group__gen.html
    COMMAND ${mkdocs_exec} build --config-file=${PROJECT_SOURCE_DIR}/mkdocs.yml
            --site-dir=$ENV{MKDOCS_SITE_DIR}
    COMMENT "building the site without building Sen"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    VERBATIM
  )
  add_dependencies(docs_preview docs_content)
  set_target_properties(docs_preview PROPERTIES FOLDER "docs")

  add_custom_target(
    mkdocs
    DEPENDS ${doc_sources} ${snippets} ${_lic_page}
    COMMAND ${mkdocs_exec} build --config-file=${PROJECT_SOURCE_DIR}/mkdocs.yml
            --site-dir=$ENV{MKDOCS_SITE_DIR}
    COMMENT "generating main site"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    VERBATIM
  )
  add_dependencies(mkdocs docs_content)

  find_program(plantuml_exec NAMES plantuml)

  # A skip here used to take add_dependencies with it, so a machine without
  # plantuml built the site with no snippet dependency and shipped 38 broken
  # includes, successfully. The failure now waits for the target that needs it,
  # so docs_preview still works: it stands a placeholder in for the diagram.
  if(plantuml_exec)
    add_custom_target(
      plantuml_output
      DEPENDS fom_uml
      COMMAND plantuml -tsvg fom.plantuml
      WORKING_DIRECTORY ${snippets_dir}
    )
  else()
    # VERBATIM, or the shell reads the parentheses in the message and reports its
    # own syntax error instead of printing why the build stopped.
    add_custom_target(
      plantuml_output
      COMMAND
        ${CMAKE_COMMAND} -E echo
        "plantuml was not found, so the site would ship without the FOM diagram. Install it (it needs a JRE), or use the docs_preview target."
      COMMAND ${CMAKE_COMMAND} -E false
      VERBATIM
    )
  endif()

  add_dependencies(mkdocs snippets_target plantuml_output)

  set_target_properties(mkdocs PROPERTIES FOLDER "docs")

else()

  message(
    FATAL_ERROR
      "SEN_BUILD_DOCS is on but mkdocs was not found. Install it into a virtual "
      "environment with 'python3 -m venv .venv-docs && .venv-docs/bin/pip install "
      "-r docs/requirements.txt', or configure with -DSEN_BUILD_DOCS=OFF."
  )

endif()
