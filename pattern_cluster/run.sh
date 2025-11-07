#!/bin/bash

export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:./build/lib/
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:./third_party/medb/lib/
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:./third_party/fftw/lib/


passed_count=0
total_CN=0
total_AD=0
total_AH=0

# Initialize the score array for each case
declare -A case_accuracy
declare -A case_clusters
declare -A case_parallel
declare -A case_peak

# Function to set all scores to zero for a case
set_zero_scores() {
    local case_num=$1
    case_accuracy["case$case_num"]=0
    case_clusters["case$case_num"]=0
    case_parallel["case$case_num"]=0
    case_peak["case$case_num"]=0
}

# Function to check if all required files exist and are not empty
check_files() {
    local i=$1
    local layout=$2
    
    # Check if the layout file exists
    if [ ! -f "$layout" ]; then
        echo "case${i} failed!" > ./output/case${i}_score.txt
        echo "Layout file not found for case${i}: $layout, assigning 0 score"
        return 1
    fi

    # Check for the presence of other necessary documents
    if [ ! -f "cases/case${i}/case${i}_param.txt" ]; then
        echo "case${i} failed!" > ./output/case${i}_score.txt
        echo "Required param files not found for case${i}, assigning 0 score"
        return 1
    fi

    # Check if param.txt file is empty
    if [ ! -s "cases/case${i}/case${i}_param.txt" ]; then
        echo "case${i} failed!" > ./output/case${i}_score.txt
        echo "Param files are empty for case${i}, assigning 0 score"
        return 1
    fi
    
    return 0
}

# Function to check pattern_center and cluster files after running the command
check_pattern_cluster_files() {
    local i=$1
    
    # Check if pattern_center file exists and is not empty
    if [ ! -f "cases/case${i}/case${i}_pattern_center.txt" ]; then
        echo "case${i} failed!" > ./output/case${i}_score.txt
        echo "Pattern center file not found for case${i}, assigning 0 score"
        return 1
    fi

    if [ ! -s "cases/case${i}/case${i}_pattern_center.txt" ]; then
        echo "Please implement pattern_cluster" >&2
        echo "case${i} failed!" > ./output/case${i}_score.txt
        echo "Pattern center file is empty for case${i}, assigning 0 score"
        return 1
    fi

    # Check if clusters file exists and is not empty
    if [ ! -f "cases/case${i}/case${i}_clusters.txt" ]; then
        echo "case${i} failed!" > ./output/case${i}_score.txt
        echo "Clusters file not found for case${i}, assigning 0 score"
        return 1
    fi

    if [ ! -s "cases/case${i}/case${i}_clusters.txt" ]; then
        echo "Please implement pattern_cluster" >&2
        echo "case${i} failed!" > ./output/case${i}_score.txt
        echo "Clusters file is empty for case${i}, assigning 0 score"
        return 1
    fi

    # Check if the first line of clusters file is not 0
    first_line=$(head -n 1 "cases/case${i}/case${i}_clusters.txt")
    if [ "$first_line" = "0" ]; then
        echo "Please implement pattern_cluster" >&2
        echo "case${i} failed!" > ./output/case${i}_score.txt
        echo "Clusters file have 0 cluster for case${i}, assigning 0 score"
        return 1
    fi
    
    return 0
}

# Function to run the evaluator and handle its output
run_evaluator() {
    local i=$1
    local layout=$2
    
    local pattern_center_file="cases/case${i}/case${i}_pattern_center.txt"
    local clusters_file="cases/case${i}/case${i}_clusters.txt"
    
    if [ ! -f "$pattern_center_file" ]; then
        touch "$pattern_center_file"
    fi

    if [ ! -f "$clusters_file" ]; then
        touch "$clusters_file"
    fi

    taskset -c 0-31 ./pattern_cluster_evaluator -layout "$layout" -param "cases/case${i}/case${i}_param.txt" -pattern_centers "cases/case${i}/case${i}_pattern_center.txt" -clusters "cases/case${i}/case${i}_clusters.txt"

    if [ $? -eq 1 ]; then
        echo "case${i} failed!" > ./output/case${i}_score.txt
    fi
}

# Function to calculate scores based on evaluator output
calculate_scores() {
    local i=$1
    local output_file=$2
    
    first_line=$(head -n 1 "$output_file" 2>/dev/null)
    if [ -z "$first_line" ]; then
        echo "Empty output file for case${i}, assigning 0 score"
        set_zero_scores $i
        return
    fi
    
    if [[ $first_line == *"failed"* ]]; then
        echo "Case${i} failed, assigning 0 score"
        set_zero_scores $i
        return
    elif [[ $first_line == *"passed"* ]]; then
        passed_count=$((passed_count + 1))
        
        # Read lines 3 to 5 from output file
        third_line=$(sed -n '3p' "$output_file")
        fourth_line=$(sed -n '4p' "$output_file")
        fifth_line=$(sed -n '5p' "$output_file")
        
        # Extract numbers using the last two fields
        x1=$(echo "$third_line" | awk '{print $(NF-1)}')
        x2=$(echo "$third_line" | awk '{print $NF}')
        t1=$(echo "$fourth_line" | awk '{print $(NF-1)}')
        t2=$(echo "$fourth_line" | awk '{print $NF}')
        m1=$(echo "$fifth_line" | awk '{print $(NF-1)}')
        m2=$(echo "$fifth_line" | awk '{print $NF}')
        
        # Validate extracted values are numbers
        if ! [[ "$x1" =~ ^[0-9]+$ ]] || ! [[ "$x2" =~ ^[0-9]+$ ]]; then
            echo "Invalid clusters numbers for case${i}: x1=$x1, x2=$x2"
            CN_score=0
        else
            CN_score=$(echo "scale=6; ($x2 - $x1) / $x2" | bc)
        fi
        
        if ! [[ "$t1" =~ ^[0-9]+\.?[0-9]*$ ]] || ! [[ "$t2" =~ ^[0-9]+\.?[0-9]*$ ]]; then
            echo "Invalid duration numbers for case${i}: t1=$t1, t2=$t2"
            AD_score=0
        else
            ratio=$(echo "scale=6; $t2 / $t1" | bc)
            if (( $(echo "$ratio > 100" | bc -l) )); then
                AD_score=1
            else
                part1=$CN_score
                part2=$(echo "scale=6; $t2 * $t2" | bc)
                part3=$(echo "scale=6; $t1 * $t1 * 10000" | bc)
                if (( $(echo "$part3 == 0" | bc -l) )); then
                    AD_score=0
                else
                    AD_score=$(echo "scale=6; $part1 * $part2 / $part3" | bc)
                fi
            fi
        fi
        
        if ! [[ "$m1" =~ ^[0-9]+\.?[0-9]*$ ]] || ! [[ "$m2" =~ ^[0-9]+\.?[0-9]*$ ]]; then
            echo "Invalid HWM numbers for case${i}: m1=$m1, m2=$m2"
            AH_score=0
        else
            if [ $i -ge 3 ] && (( $(echo "$m1 < $m2" | bc -l) )); then
                AH_score=1
            else
                AH_score=0
            fi
        fi
        
        # Set weights based on case number
        if [ $i -le 2 ]; then
            n=2
            m=1
        else
            n=5
            m=2
        fi
        
        # Calculate values
        cn_value=$(echo "scale=6; $CN_score * $n" | bc)
        ad_value=$(echo "scale=6; $AD_score * $m" | bc)
        ah_value=$(echo "scale=6; $AH_score * 1.5" | bc)
        
        # Accumulate totals
        total_CN=$(echo "scale=6; $total_CN + $cn_value" | bc)
        total_AD=$(echo "scale=6; $total_AD + $ad_value" | bc)
        total_AH=$(echo "scale=6; $total_AH + $ah_value" | bc)
        
        # Record the score for each case
        case_accuracy["case$i"]=10
        case_clusters["case$i"]=$cn_value
        case_parallel["case$i"]=$ad_value
        case_peak["case$i"]=$ah_value
    else
        echo "Unexpected first line in ${output_file}: ${first_line}, assigning 0 score"
        set_zero_scores $i
    fi
}

# Function to initialize score arrays
init_score_arrays() {
    declare -gA case_accuracy
    declare -gA case_clusters
    declare -gA case_parallel
    declare -gA case_peak
    
    for i in {1..6}; do
        case_accuracy["case$i"]=0
        case_clusters["case$i"]=0
        case_parallel["case$i"]=0
        case_peak["case$i"]=0
    done
}

# Function to update the score report after each case
update_score_report() {
    # Calculate total scores
    part1=$(echo "scale=6; $passed_count * 10" | bc)
    total_score=$(echo "scale=6; $part1 + $total_CN + $total_AD + $total_AH" | bc)

    # Calculate percentages
    total_percent=$(echo "scale=6; $total_score * 100 / 100" | bc)
    accuracy_percent=$(echo "scale=6; $part1 * 100 / 60" | bc)
    clusters_percent=$(echo "scale=6; $total_CN * 100 / 24" | bc)
    parallel_percent=$(echo "scale=6; $total_AD * 100 / 10" | bc)
    peak_percent=$(echo "scale=6; $total_AH * 100 / 6" | bc)

    # Calculate the percentage for each case
    declare -A case_accuracy_percent
    declare -A case_clusters_percent
    declare -A case_parallel_percent
    declare -A case_peak_percent

    for i in {1..6}; do
        case_accuracy_percent["case$i"]=$(echo "scale=6; ${case_accuracy["case$i"]} * 100 / 10" | bc)
        
        if [ $i -le 2 ]; then
            full_score_cn=2
            full_score_ad=1
        else
            full_score_cn=5
            full_score_ad=2
        fi
        
        if [[ ${case_clusters["case$i"]} ]]; then
            case_clusters_percent["case$i"]=$(echo "scale=6; ${case_clusters["case$i"]} * 100 / $full_score_cn" | bc)
        else
            case_clusters_percent["case$i"]=0
        fi
        
        if [[ ${case_parallel["case$i"]} ]]; then
            case_parallel_percent["case$i"]=$(echo "scale=6; ${case_parallel["case$i"]} * 100 / $full_score_ad" | bc)
        else
            case_parallel_percent["case$i"]=0
        fi
        
        if [ $i -ge 3 ]; then
            full_score_ah=1.5
            if [[ ${case_peak["case$i"]} ]]; then
                case_peak_percent["case$i"]=$(echo "scale=6; ${case_peak["case$i"]} * 100 / $full_score_ah" | bc)
            else
                case_peak_percent["case$i"]=0
            fi
        else
            case_peak_percent["case$i"]=0
        fi
    done

    # Output the result in CSV
    {
        echo "Category,Total Score,Run Score,Score Ratio(%)"
        echo "Total Score,100,$total_score,$total_percent"
        echo "Accuracy,60,$part1,$accuracy_percent"
        for i in {1..6}; do
            echo "  Case $i,10,${case_accuracy["case$i"]},${case_accuracy_percent["case$i"]}"
        done

        echo "Clusters,24,$total_CN,$clusters_percent"
        for i in {1..6}; do
            if [ $i -le 2 ]; then
                full_score=2
            else
                full_score=5
            fi
            echo "  Case $i,$full_score,${case_clusters["case$i"]},${case_clusters_percent["case$i"]}"
        done

        echo "Parallel,10,$total_AD,$parallel_percent"
        for i in {1..6}; do
            if [ $i -le 2 ]; then
                full_score=1
            else
                full_score=2
            fi
            echo "  Case $i,$full_score,${case_parallel["case$i"]},${case_parallel_percent["case$i"]}"
        done

        echo "Peak,6,$total_AH,$peak_percent"
        for i in {1..6}; do
            if [ $i -ge 3 ]; then
                full_score=1.5
                echo "  Case $i,$full_score,${case_peak["case$i"]},${case_peak_percent["case$i"]}"
            fi
        done
    } > ./output/score.csv

    echo "Updated score report after case processing"
}

# Main flow
mkdir -p ./output

# Initialize score arrays
init_score_arrays

# Initialize total counters
passed_count=0
total_CN=0
total_AD=0
total_AH=0

for i in {1..6}; do
    case $i in
        1) layout="cases/layouts/small_layout_csc.hgs" ;;
        2) layout="cases/layouts/small_layout_ecc.hgs" ;;
        3|4) layout="cases/layouts/large_layout.hgs" ;;
        5|6) layout="cases/layouts/hidden_layout.hgs" ;;
    esac

    # Check files
    if ! check_files $i $layout; then
        set_zero_scores $i
        update_score_report
        continue
    fi

    # Run evaluator
    run_evaluator $i $layout

    # Check pattern_center and cluster files after running the command
    if ! check_pattern_cluster_files $i; then
        set_zero_scores $i
        update_score_report
        continue
    fi

    # Find output file
    # output_file=$(find ./output/ -name "case${i}_score*" -type f | head -n 1)
    output_file=$(find ./output/ -name "case${i}_score.txt" -type f | head -n 1)
    
    if [ -z "$output_file" ]; then
        echo "No output file found for case${i}, assigning 0 score"
        set_zero_scores $i
        update_score_report
        continue
    fi
    
    # Calculate scores
    calculate_scores $i $output_file
    
    # Update the score report after each case
    update_score_report
done

echo "The total score file completes and outputs normally: output/score.csv"
# # Function to generate the final score report
# generate_score_report() {
#     # Calculate total scores
#     part1=$(echo "scale=6; $passed_count * 10" | bc)
#     total_score=$(echo "scale=6; $part1 + $total_CN + $total_AD + $total_AH" | bc)

#     # Calculate percentages
#     total_percent=$(echo "scale=6; $total_score * 100 / 100" | bc)
#     accuracy_percent=$(echo "scale=6; $part1 * 100 / 60" | bc)
#     clusters_percent=$(echo "scale=6; $total_CN * 100 / 24" | bc)
#     parallel_percent=$(echo "scale=6; $total_AD * 100 / 10" | bc)
#     peak_percent=$(echo "scale=6; $total_AH * 100 / 6" | bc)

#     # Calculate the percentage for each case
#     declare -A case_accuracy_percent
#     declare -A case_clusters_percent
#     declare -A case_parallel_percent
#     declare -A case_peak_percent

#     for i in {1..6}; do
#         case_accuracy_percent["case$i"]=$(echo "scale=6; ${case_accuracy["case$i"]} * 100 / 10" | bc)
        
#         if [ $i -le 2 ]; then
#             full_score_cn=2
#             full_score_ad=1
#         else
#             full_score_cn=5
#             full_score_ad=2
#         fi
        
#         if [[ ${case_clusters["case$i"]} ]]; then
#             case_clusters_percent["case$i"]=$(echo "scale=6; ${case_clusters["case$i"]} * 100 / $full_score_cn" | bc)
#         else
#             case_clusters_percent["case$i"]=0
#         fi
        
#         if [[ ${case_parallel["case$i"]} ]]; then
#             case_parallel_percent["case$i"]=$(echo "scale=6; ${case_parallel["case$i"]} * 100 / $full_score_ad" | bc)
#         else
#             case_parallel_percent["case$i"]=0
#         fi
        
#         if [ $i -ge 3 ]; then
#             full_score_ah=1.5
#             if [[ ${case_peak["case$i"]} ]]; then
#                 case_peak_percent["case$i"]=$(echo "scale=6; ${case_peak["case$i"]} * 100 / $full_score_ah" | bc)
#             else
#                 case_peak_percent["case$i"]=0
#             fi
#         else
#             case_peak_percent["case$i"]=0
#         fi
#     done

#     # Output the result in CSV
#     {
#         echo "Category,Total Score,Run Score,Score Ratio(%)"
#         echo "Total Score,100,$total_score,$total_percent"
#         echo "Accuracy,60,$part1,$accuracy_percent"
#         for i in {1..6}; do
#             echo "  Case $i,10,${case_accuracy["case$i"]},${case_accuracy_percent["case$i"]}"
#         done

#         echo "Clusters,24,$total_CN,$clusters_percent"
#         for i in {1..6}; do
#             if [ $i -le 2 ]; then
#                 full_score=2
#             else
#                 full_score=5
#             fi
#             echo "  Case $i,$full_score,${case_clusters["case$i"]},${case_clusters_percent["case$i"]}"
#         done

#         echo "Parallel,10,$total_AD,$parallel_percent"
#         for i in {1..6}; do
#             if [ $i -le 2 ]; then
#                 full_score=1
#             else
#                 full_score=2
#             fi
#             echo "  Case $i,$full_score,${case_parallel["case$i"]},${case_parallel_percent["case$i"]}"
#         done

#         echo "Peak,6,$total_AH,$peak_percent"
#         for i in {1..6}; do
#             if [ $i -ge 3 ]; then
#                 full_score=1.5
#                 echo "  Case $i,$full_score,${case_peak["case$i"]},${case_peak_percent["case$i"]}"
#             fi
#         done
#     } > ./output/score.csv

#     echo "The total score file completes and outputs normally: output/score.csv"
# }

# # Main flow
# mkdir -p ./output

# for i in {1..6}; do
#     case $i in
#         1) layout="cases/layouts/small_layout_csc.oas" ;;
#         2) layout="cases/layouts/small_layout_ecc.oas" ;;
#         3|4) layout="cases/layouts/large_layout.oas" ;;
#         5|6) layout="cases/layouts/hidden_layout.oas" ;;
#     esac

#     # Check files
#     if ! check_files $i $layout; then
#         set_zero_scores $i
#         continue
#     fi

#     # Run evaluator
#     run_evaluator $i $layout

#     # Check pattern_center and cluster files after running the command
#     if ! check_pattern_cluster_files $i; then
#         set_zero_scores $i
#         continue
#     fi

#     # Find output file
#     output_file=$(find ./output/ -name "case${i}_score*" -type f | head -n 1)
    
#     if [ -z "$output_file" ]; then
#         echo "No output file found for case${i}, assigning 0 score"
#         set_zero_scores $i
#         continue
#     fi
    
#     # Calculate scores
#     calculate_scores $i $output_file
# done

# # Generate final report
# generate_score_report