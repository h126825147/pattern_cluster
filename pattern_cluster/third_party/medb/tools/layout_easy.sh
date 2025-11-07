#!/bin/bash
set +xe

BASE_PATH=$(cd "$(dirname $0)" || exit; pwd)
PROJECT_PATH=${BASE_PATH}/..
BUILD_PATH=${PROJECT_PATH}/build/medb/bin/medb_sdv
BIN_PATH=${PROJECT_PATH}/bin/medb_sdv
if [ -f ${BUILD_PATH} ] ; then
    PROGRAM_PATH=${BUILD_PATH}
elif [ -f ${BIN_PATH} ] ; then
    PROGRAM_PATH=${BIN_PATH}
else
    echo "program does not exist!"
    exit
fi

function LoadData() {
    for (( i = 0; i < ${#param_arr[@]}; ++i )); do
        result=${param_arr[$i]}
        if [ "$result" = "-input_file" ] ; then
            ((++i))
            inputfile=${param_arr[$i]}
        elif [ "$result" = "-output_file" ] ; then
            ((++i))
            outputfile=${param_arr[$i]}
        fi
    done
    ${PROGRAM_PATH} import -o $outputfile -i $inputfile
    echo ""
    if [ ! -n "$outputfile" ] ; then
        echo "\"LoadData\" Failed!"
    fi
}

function FlattenCell() {
    for (( i = 0; i < ${#param_arr[@]}; ++i )); do
        result=${param_arr[$i]}
        if [ "$result" = "-input_file" ] ; then
            ((++i))
            inputfile=${param_arr[$i]}
        elif [ "$result" = "-output_file" ] ; then
            ((++i))
            outputfile=${param_arr[$i]}
        fi
    done

    ${PROGRAM_PATH} layout --input -i $inputfile \
    --flattencell -topcell TOP \
    --exportdata -o $outputfile -no-binning
    
    echo ""
    if [ ! -n "$outputfile" ] ; then
        echo "\"FlattenCell\" Failed!"
    fi
}

function Compare() {
    layer_arr=()
    j=0
    for (( i = 0; i < ${#param_arr[@]}; ++i )); do
        result=${param_arr[$i]}
        if [ "$result" = "-layer" ] ; then
            ((++i))
            while [[ ${param_arr[$i]} =~ / ]]
            do
                layer_arr[$j]="${param_arr[$i]}"
                ((++j))
                ((++i))
            done
            ((--i))
        elif [ "$result" = "-input_file" ] ; then
            ((++i))
            inputfile1=${param_arr[$i]}
            ((++i))
            inputfile2=${param_arr[$i]}
        elif [ "$result" = "-output_file" ] ; then
            ((++i))
            outputfile=${param_arr[$i]}
        fi
    done
    
    length=${#layer_arr[@]}
    inlayer=""
    for (( i = 0; i < $length; ++i )); do
        inlayer=$inlayer${layer_arr[$i]}","
    done
    inlayer=${inlayer%','*}

    ${PROGRAM_PATH} compare -i $inputfile1 $inputfile2 -o $outputfile -layer $inlayer

    echo ""
    if [ ! -n "$outputfile" ] ; then
        echo "\"Compare\" Failed!"
    fi
}

function MoveLayer() {
    layer_arr=()
    j=0
    for (( i = 0; i < ${#param_arr[@]}; ++i )); do
        result=${param_arr[$i]}
        if [ "$result" = "-layer" ] ; then
            ((++i))
            while [[ ${param_arr[$i]} =~ / ]]
            do
                layer_arr[$j]="${param_arr[$i]}"
                ((++j))
                ((++i))
            done
            ((--i))
        elif [ "$result" = "-input_file" ] ; then
            ((++i))
            inputfile=${param_arr[$i]}
        elif [ "$result" = "-output_file" ] ; then
            ((++i))
            outputfile=${param_arr[$i]}
        elif [ "$result" = "-offset" ] ; then
            ((++i))
            offset=${param_arr[$i]}
        fi
    done

    length=${#layer_arr[@]}
    if [ "$offset" != "" ] ; then
        inlayer=""
        for (( i = 0; i < $length; ++i )); do
            inlayer=$inlayer${layer_arr[$i]}","
        done
        inlayer=${inlayer%','*}

        ${PROGRAM_PATH} layout --input -i $inputfile \
        --flattencell -topcell TOP \
        --movelayers -layer $inlayer -offset $offset \
        --exportdata -o $outputfile
    else
        ${PROGRAM_PATH} layout --input -i $inputfile \
        --flattencell -topcell TOP \
        --movelayer -from ${layer_arr[0]} -to ${layer_arr[1]} \
        --exportdata -o $outputfile
    fi
    echo ""
    if [ ! -n "$outputfile" ] ; then
        echo "\"MoveLayer\" Failed!"
    fi
}

function LayerBinning() {
    for (( i = 0; i < ${#param_arr[@]}; ++i )); do
        result=${param_arr[$i]}
        if [ "$result" = "-layer" ] ; then
            ((++i))
            layer_1=${param_arr[$i]}
            ((++i))
            layer_2=${param_arr[$i]}
        elif [ "$result" = "-input_file" ] ; then
            ((++i))
            inputfile=${param_arr[$i]}
        elif [ "$result" = "-output_file" ] ; then
            ((++i))
            outputfile=${param_arr[$i]}
        fi
    done

    ${PROGRAM_PATH} layout --input -i $inputfile \
    --flattencell -topcell TOP \
    --layerbinning -in $layer_1 -out $layer_2  \
    --exportdata -o $outputfile -no-binning
    echo ""
    if [ ! -n "$outputfile" ] ; then
        echo "\"LayerBinning\" Failed!"
    fi
}

function CloneLayer() {
    layer_arr=()
    j=0
    for (( i = 0; i < ${#param_arr[@]}; ++i )); do
        result=${param_arr[$i]}
        if [ "$result" = "-layer" ] ; then
            ((++i))
            while [[ ${param_arr[$i]} =~ / ]]
            do
                layer_arr[$j]="${param_arr[$i]}"
                ((++j))
                ((++i))
            done
            ((--i))
        elif [ "$result" = "-input_file" ] ; then
            ((++i))
            inputfile_1=${param_arr[$i]}
            ((++i))
            if [[ ${param_arr[$i]} =~ \. ]] ; then
                inputfile_2=${param_arr[$i]}
            else
                ((--i))
            fi
        elif [ "$result" = "-output_file" ] ; then
            ((++i))
            outputfile=${param_arr[$i]}
        fi
    done    

    if [ ! -n "$inputfile_2" ] ; then
        length=${#layer_arr[@]}
        for (( i = 0; i < $length; ++i )); do
            if [ "$i" -eq "$[$length-1]" ] ; then
                lastlayer=${layer_arr[$i]}
            else
                inlayer=$inlayer${layer_arr[$i]}","
            fi
        done
        inlayer=${inlayer%','*}
        ${PROGRAM_PATH} clone -des_layer $lastlayer -o $outputfile -i $inputfile_1 -layer $inlayer
    else
        length=${#layer_arr[@]}
        for (( i = 0; i < $length; ++i )); do
            inlayer=$inlayer${layer_arr[$i]}","
        done
        inlayer=${inlayer%','*}
        ${PROGRAM_PATH} clone -des $inputfile_2 -o $outputfile -i $inputfile_1 -layer $inlayer
    fi
    echo ""
    if [ ! -n "$outputfile" ] ; then
        echo "\"CloneLayer\" Failed!"
    fi
}

function LayerMerge() {
    layer_arr=()
    j=0
    for (( i = 0; i < ${#param_arr[@]}; ++i )); do
        result=${param_arr[$i]}
        if [ "$result" = "-layer" ] ; then
            ((++i))
            while [[ ${param_arr[$i]} =~ / ]]
            do
                layer_arr[$j]="${param_arr[$i]}"
                ((++j))
                ((++i))
            done
            ((--i))
        elif [ "$result" = "-input_file" ] ; then
            ((++i))
            inputfile=${param_arr[$i]}
        elif [ "$result" = "-output_file" ] ; then
            ((++i))
            outputfile=${param_arr[$i]}
        fi
    done

    length=${#layer_arr[@]}
    for (( i = 0; i < $length; ++i )); do
        if [ "$i" -eq "$[$length-1]" ] ; then
            lastlayer=${layer_arr[$i]}
        else
            inlayer=$inlayer${layer_arr[$i]}","
        fi
    done
    inlayer=${inlayer%','*}

    ${PROGRAM_PATH} layout --input -i $inputfile \
    --flattencell -topcell TOP \
    --layermerge -inlayers $inlayer -outlayer $lastlayer \
    --exportdata -o $outputfile
    echo ""
    if [ ! -n "$outputfile" ] ; then
        echo "\"LayerMerge\" Failed!"
    fi
}

function DoResize() {
    for (( i = 0; i < ${#param_arr[@]}; ++i )); do
        result=${param_arr[$i]}
        if [ "$result" = "-layer" ] ; then
            ((++i))
            layer_1=${param_arr[$i]}
            ((++i))
            layer_2=${param_arr[$i]}
        elif [ "$result" = "-size" ] ; then
            ((++i))
            size=${param_arr[$i]}
        elif [ "$result" = "-input_file" ] ; then
            ((++i))
            inputfile=${param_arr[$i]}
        elif [ "$result" = "-output_file" ] ; then
            ((++i))
            outputfile=${param_arr[$i]}
        fi
    done
    ${PROGRAM_PATH} layout --input -i $inputfile \
    --flattencell -topcell TOP \
    --doresize -from $layer_1 -to $layer_2 -delta $size  \
    --exportdata -o $outputfile
    echo ""
    if [ ! -n "$outputfile" ] ; then
        echo "\"DoResize\" Failed!"
    fi
}

function DoBoolean() {
    op=`echo ${param_arr[1],,}`
    layer_arr=()
    j=0
    for (( i = 0; i < ${#param_arr[@]}; ++i )); do
        result=${param_arr[$i]}
        if [ "$result" = "-layer" ] ; then
            ((++i))
            while [[ ${param_arr[$i]} =~ / ]]
            do
                layer_arr[$j]="${param_arr[$i]}"
                ((++j))
                ((++i))
            done
            ((--i))
        elif [ "$result" = "-input_file" ] ; then
            ((++i))
            inputfile=${param_arr[$i]}
        elif [ "$result" = "-output_file" ] ; then
            ((++i))
            outputfile=${param_arr[$i]}
        fi
    done

    length=${#layer_arr[@]}
    if [ "$length" -gt "3" ] ; then
        inlayer=""
        for (( i = 0; i < $length; ++i )); do
            if [ "$i" -eq "$[$length-1]" ] ; then
                lastlayer=${layer_arr[$i]}
            else
                inlayer=$inlayer${layer_arr[$i]}","
            fi
        done
        inlayer=${inlayer%','*}
        ${PROGRAM_PATH} layout --input -i $inputfile \
        --flattencell -topcell TOP \
        --dobooleans -operation $op -inlayers $inlayer -outlayer $lastlayer \
        --exportdata -o $outputfile
    else
        ${PROGRAM_PATH} layout --input -i $inputfile \
        --flattencell -topcell TOP \
        --doboolean -operation $op -layer_a ${layer_arr[0]} -layer_b ${layer_arr[1]} -outlayer ${layer_arr[2]} \
        --exportdata -o $outputfile
    fi
    echo ""
    if [ ! -n "$outputfile" ] ; then
        echo "\"DoBoolean\" Failed!"
    fi
}

function Help() {
    echo "usage: layout_easy [operation] [-layer layer_parameter] [-size number]"
    echo $'\t'"           ""[-offset number] [-input_file path] [-output_file path]"
    echo ""

    echo "The main types of operators are as follows:"

    echo $'\t'"LoadData""       "$'\t'"Import and export layout files. The following parameters must be:"
    echo "                      "$'\t'"[-input_file]:The path and file name of the layout"
    echo "                      "$'\t'"[-output_file]:Path and file name of exported layout"
    echo "                      "$'\t'"e.g."
    echo "                      "$'\t'"   ./layout_easy.sh LoadData -input_file ./test1.gds -output_file ./test2.gds"

    echo ""
    echo $'\t'"FlattenCell""    "$'\t'"Flatten the layout cell"
    echo "                      "$'\t'"[-input_file]:Layout files to be flattened"
    echo "                      "$'\t'"[-output_file]:Layout file after flattening"
    echo "                      "$'\t'"e.g."
    echo "                      "$'\t'"   ./layout_easy.sh FlattenCell -input_file ./test1.gds -output_file ./test2.gds"

    echo ""
    echo $'\t'"MoveLayer""      "$'\t'"Move data from one layer or multiple layers to another layer"
    echo "                      "$'\t'"[-layer]:Layer for operation.the first is the input layer"
    echo "                      "$'\t'"         ""and the second is the output layer"
    echo "                      "$'\t'"[-offset]:Displacement deviation of layer.When this parameter exists,"
    echo "                      "$'\t'"         ""input layer plus [-offset] becomes output layer"
    echo "                      "$'\t'"[-input_file]:The path and file name of the layout"
    echo "                      "$'\t'"[-output_file]:Path and file name of exported layout"
    echo "                      "$'\t'"e.g."
    echo "                      "$'\t'"   1 ./layout_easy.sh MoveLayer -layer 1/0 20/0 -input_file ./test1.gds"
    echo "                      "$'\t'"     ""-output_file ./test2.gds"
    echo "                      "$'\t'"   2 ./layout_easy.sh MoveLayer -layer 1/0 2/0 3/0 -offset 30 -input_file ./test1.gds"
    echo "                      "$'\t'"     ""-output_file ./test2.gds"

    echo ""
    echo $'\t'"CloneLayer""     "$'\t'"Copy layer data to another layer or layout file"
    echo "                      "$'\t'"[-layer]:Layer for operation.The following parameters must be more"
    echo "                      "$'\t'"         ""than two"
    echo "                      "$'\t'"         ""When there is only one [-input_file] parameter,the first"
    echo "                      "$'\t'"         ""is the input layer and the second is the output layer"
    echo "                      "$'\t'"         ""When there are two [-input_file] parameters:"
    echo "                      "$'\t'"         ""copy the layer of the first file to the second layer. If"
    echo "                      "$'\t'"         ""the second file layer exists and has data, merge again"
    echo "                      "$'\t'"[-input_file]:The path and file name of the layout,refer to the"
    echo "                      "$'\t'"         ""previous parameter for details"
    echo "                      "$'\t'"[-output_file]:Path and file name of exported layout"
    echo "                      "$'\t'"e.g."
    echo "                      "$'\t'"   1 ./layout_easy.sh CloneLayer -layer 1/0 2/0 50/0 -input_file ./test1.gds"
    echo "                      "$'\t'"     ""-output_file ./test2.gds"
    echo "                      "$'\t'"   2 ./layout_easy.sh CloneLayer -layer 1/0 2/0 -input_file ./test1.gds ./test2.gds"
    echo "                      "$'\t'"     ""-output_file ./test3.gds"

    echo ""
    echo $'\t'"LayerBinning""   "$'\t'"The result of OR operation on the data in the input layer is stored"
    echo "                      "$'\t'"         ""in the output layer"
    echo "                      "$'\t'"[-layer]:Two parameters are required,the first is the input layer"
    echo "                      "$'\t'"         ""and the second is the output layer"
    echo "                      "$'\t'"[-input_file]:The path and file name of the layout"
    echo "                      "$'\t'"[-output_file]:Path and file name of exported layout"
    echo "                      "$'\t'"e.g."
    echo "                      "$'\t'"   ./layout_easy.sh LayerBinning -layer 1/0 10/0 -input_file ./test1.gds"
    echo "                      "$'\t'"   ""-output_file ./test2.gds"

    echo ""
    echo $'\t'"LayerMerge""     "$'\t'"The result of OR operation on the data in the input layer is stored"
    echo "                      "$'\t'"         ""in the output layer"
    echo "                      "$'\t'"[-layer]:Must exceed three parameters.The last is the output layer,"
    echo "                      "$'\t'"         ""and the rest is the input layer"
    echo "                      "$'\t'"[-input_file]:The path and file name of the layout"
    echo "                      "$'\t'"[-output_file]:Path and file name of exported layout"
    echo "                      "$'\t'"e.g."
    echo "                      "$'\t'"   ./layout_easy.sh LayerMerge -layer 1/0 2/0 10/0 -input_file ./test1.gds"
    echo "                      "$'\t'"   ""-output_file ./test2.gds"

    echo ""
    echo $'\t'"Compare""        "$'\t'"The self layout compares the data in the specified layer with other"
    echo "                      "$'\t'"         ""layouts, and the result is output as a new layout file"
    echo "                      "$'\t'"[-layer]:Layers to compare"
    echo "                      "$'\t'"[-input_file]:The path and file name of the layout.Two parameters are required"
    echo "                      "$'\t'"[-output_file]:Path and file name of exported layout"
    echo "                      "$'\t'"e.g."
    echo "                      "$'\t'"   ./layout_easy.sh Compare -layer 1/0 10/0 -input_file ./test1.gds ./test2.gds"
    echo "                      "$'\t'"   ""-output_file ./test3.gds"

    echo ""
    echo $'\t'"DoResize""       "$'\t'"Scale the layout file"
    echo "                      "$'\t'"[-layer]:Two parameters are required.The first layer is the layer to do"
    echo "                      "$'\t'"         ""\"DoResie\", and the second layer is the layer after \"DoResie\""
    echo "                      "$'\t'"[-size]:Do multiple of \"DoResize\""
    echo "                      "$'\t'"[-input_file]:The path and file name of the layout"
    echo "                      "$'\t'"[-output_file]:Path and file name of exported layout"
    echo "                      "$'\t'"e.g."
    echo "                      "$'\t'"   ./layout_easy.sh DoResize -layer 1/0 20/0 -size 5 -input_file ./test1.gds"
    echo "                      "$'\t'"   ""-output_file ./test2.gds"

    echo ""
    echo $'\t'"DoBoolean""      "$'\t'"Do \"DoBoolean\" operation on [-layer] data except the last one, and put"
    echo "                      "$'\t'"         ""the result on the last parameter of [-layer]"
    echo "                      "$'\t'"\"Doboolean\" includes four types, which are case insensitive.They include"
    echo "                      "$'\t'"         ""or, sub, xor, an"
    echo "                      "$'\t'"[-layer]:The following parameters must be more than three"
    echo "                      "$'\t'"         ""When there are three parameters, the first and second are used"
    echo "                      "$'\t'"         ""as input, the third is used as output,"
    echo "                      "$'\t'"         ""and when there are more than three parameters, the last is used"
    echo "                      "$'\t'"         ""as output layer"
    echo "                      "$'\t'"[-input_file]:The path and file name of the layout"
    echo "                      "$'\t'"[-output_file]:Path and file name of exported layout"
    echo "                      "$'\t'"e.g."
    echo "                      "$'\t'"   ./layout_easy.sh DoBoolean or -layer 1/0 2/0 3/0 -input_file ./test1.gds"
    echo "                      "$'\t'"   ""-output_file ./test2.gds"
}

if [ $# -gt 0 ]; then
    operator=$1
    param_arr=()
    i=0
    while [ $# -ne 0 ]
    do
        param_arr[$i]=$1
        ((++i))
        shift
    done
    case "$operator" in
        "LoadData")
            LoadData
            ;;
        "FlattenCell")
            FlattenCell
            ;;
        "MoveLayer")
            MoveLayer
            ;;
        "CloneLayer")
            CloneLayer
            ;;
        "LayerBinning")
            LayerBinning
            ;;
        "LayerMerge")
            LayerMerge
            ;;
        "Compare")
            Compare
            ;;
        "DoResize")
            DoResize
            ;;    
        "DoBoolean")
            DoBoolean
            ;;
        "-h")
            Help
            ;;
        "-help")
            Help
            ;;
        *)
        echo "This operation is not supported!"
    esac
else
   echo "Please enter parameters or use -h or -help to view help"
fi
