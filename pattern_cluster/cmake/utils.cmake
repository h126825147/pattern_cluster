# cmake utils
function(set_target_build_dir name)

endfunction()

function(download_package back_url output_path)
  set(pre_url CACHE STRING "pre_url")

  set(urls "http://192.168.0.173/" "http://121.36.231.169/")

  set(max_retries 3)
  set(sleep_interval 5)
  set(retry_count 0)
  set(success 0)

  while(retry_count LESS max_retries AND NOT success)
    foreach(url ${urls})
      execute_process(
        COMMAND
          curl -s --head --user $ENV{INNER_RELEASE_USER}:$ENV{INNER_RELEASE_PWD}
          -o /dev/null -w "%{http_code}" --connect-timeout 3 ${url}
        OUTPUT_VARIABLE http_status)
      if(http_status EQUAL 200)
        set(pre_url ${url})
        set(success 1)
        message(STATUS "The download url is ${pre_url}")
        break()
      endif()
    endforeach()
    if(NOT success)
      math(EXPR retry_count "${retry_count} + 1")
      if(retry_count LESS max_retries)
        message(
          STATUS
            "Failed to connect to ${urls} (Attempt ${retry_count} of ${max_retries}), retrying in ${sleep_interval} seconds..."
        )
        execute_process(COMMAND sleep ${sleep_interval})
      else()
        message(
          FATAL_ERROR
            "No valid URL found in ${urls} (Attempt ${retry_count} of ${max_retries}) with status code: ${http_status}"
        )
      endif()
    endif()
  endwhile()

  set(max_retries 3)
  set(sleep_interval 5)
  set(retry_count 0)
  set(success 0)
  set(full_url "${pre_url}${back_url}")
  message(STATUS "Downloading package from ${full_url}")

  while(retry_count LESS max_retries AND NOT success)
    execute_process(
      COMMAND
        curl -s --head --user $ENV{INNER_RELEASE_USER}:$ENV{INNER_RELEASE_PWD}
        -o /dev/null -w "%{http_code}" --connect-timeout 3 ${full_url}
      OUTPUT_VARIABLE http_status)
    if(http_status EQUAL 200)
      set(success 1)
    else()
      math(EXPR retry_count "${retry_count} + 1")
      if(retry_count LESS max_retries)
        message(
          STATUS
            "Failed to download package ${full_url} (Attempt ${retry_count} of ${max_retries}), retrying in ${sleep_interval} seconds..."
        )
        execute_process(COMMAND sleep ${sleep_interval})
      else()
        message(
          FATAL_ERROR
            "Failed to download package ${full_url} after ${max_retries} attempts, status code: ${http_status}"
        )
      endif()
    endif()
  endwhile()

  set(max_retries 3)
  set(sleep_interval 5)
  set(retry_count 0)
  set(success 0)
  if(http_status EQUAL 200)
    while(retry_count LESS max_retries AND success EQUAL 0)
      math(EXPR this_count "${retry_count} + 1")
      message(
        STATUS
          "Try \"curl -k -L --retry 1 --user $ENV{INNER_RELEASE_USER}:$ENV{INNER_RELEASE_PWD} -o ${output_path} --connect-timeout 3 ${full_url}\" attempt ${this_count} of ${max_retries}"
      )
      execute_process(
        COMMAND
          curl -k -L --retry 1 --user
          $ENV{INNER_RELEASE_USER}:$ENV{INNER_RELEASE_PWD} -o ${output_path}
          --connect-timeout 3 ${full_url}
        RESULT_VARIABLE result)
      if(result EQUAL 0)
        set(success 1)
      else()
        math(EXPR retry_count "${retry_count} + 1")
        if(retry_count LESS max_retries)
          message(
            WARNING "Download failed, retrying in ${sleep_interval} seconds...")
          execute_process(COMMAND sleep ${sleep_interval})
        endif()
      endif()
    endwhile()
  else()
    message(
      FATAL_ERROR
        "URL is not valid: ${full_url} with status code: ${http_status}")
  endif()

  if(success EQUAL 0)
    message(
      FATAL_ERROR
        "Failed to download package ${full_url} after ${max_retries} attempts")
  endif()
endfunction()

function(get_url_from_map url_map key return_var)
  list(FIND url_map ${key} index)
  if(NOT index EQUAL -1)
    math(EXPR next_index "${index} + 1")
    list(GET url_map ${next_index} package_url)
    set(${return_var}
        ${package_url}
        PARENT_SCOPE)
  else()
    message(FATAL_ERROR "Key ${key} not found in URL map")
  endif()
endfunction()

function(set_package_url processor_architecture url_map return_var)
  if(${processor_architecture} STREQUAL "x86_64" OR ${processor_architecture}
                                                    STREQUAL "i686")
    get_url_from_map("${url_map}" "x86" url)
    set(${return_var}
        ${url}
        PARENT_SCOPE)
  elseif(${processor_architecture} STREQUAL "aarch64")
    if(ENABLE_SVE)
      get_url_from_map("${url_map}" "arm_920B" url)
      set(${return_var}
          ${url}
          PARENT_SCOPE)
    else()
      get_url_from_map("${url_map}" "arm_920" url)
      set(${return_var}
          ${url}
          PARENT_SCOPE)
    endif()
  else()
    message(
      FATAL_ERROR
        "Unsupported processor architecture, only x86 and arm architectures are supported"
    )
  endif()
endfunction()
