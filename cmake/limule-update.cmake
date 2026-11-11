function(is_limule_update_build output repo branch event ref github_repository)
    set(enabled 0)
    if(repo MATCHES "^(https://github\\.com/|git@github\\.com:)Zangetsu38/Vita3K(\\.git)?/?$"
        AND NOT event STREQUAL "pull_request")
        if(branch STREQUAL "limule" OR (branch STREQUAL "HEAD"
            AND event STREQUAL "push" AND ref STREQUAL "refs/heads/limule"
            AND github_repository STREQUAL "Zangetsu38/Vita3K"))
            set(enabled 1)
        endif()
    endif()
    set(${output} ${enabled} PARENT_SCOPE)
endfunction()