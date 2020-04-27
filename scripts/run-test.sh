
location=""
storage=""
pulsar_cpus=1
topics=1
pulsar_threads=1
asio_threads=1
restart_pulsar=false
docker_cpus=`nproc`
messages_per_second=100
duration=300

disk_path=/mnt/ramdisk/pulsar/pdata

pulsartest_bin=/var/local/build/pulsartests-Desktop_Qt_5_14_0_GCC_64bit-Release/pulsartests

usage()
{
    echo "Usage: $(basename $0) [options]"
    echo
    echo "Options:"
    echo
}


while [ $# -gt 0 ];  do
    case "$1" in
        --messages-per-second)
            shift
            messages_per_second=$1
            shift
            ;;
        --duration)
            shift
            duration=$1
            shift
            ;;
        --location)
            shift
            location=$1
            shift
            ;;
        --storage)
            shift
            storage=$1
            shift
            ;;
        --pulsar-cpus)
            shift
            pulsar_cpus=$1
            shift
            ;;
        --docker-cpus)
            shift
            docker_cpus=$1
            shift
            ;;
        --topics)
            shift
            topics=$1
            shift
            ;;
        --pulsar-threads)
            shift
            pulsar_threads=$1
            shift
            ;;
        --asio-threads)
            shift
            asio_threads=$1
            shift
            ;;
        --disk-path)
            shift
            disk_path=$1
            shift
            ;;
        --pulsartest)
            shift
            pulsartest_bin=$1
            shift
            ;;
        --restart-pulsar)
            shift
            restart_pulsar=true
            ;;        
            
        *)
            echo "ERROR: Unknown parameter: $1"
            echo
            usage
            exit 1
            ;;
    esac
done

if $restart_pulsar; then

    if [ -z "${disk_path}" ] ; then
        echo "Missing path to pulsar data volume (--disk-path)"
        exit -1
    fi

    echo "Preparing disk"
    sudo mkdir -p "${disk_path}"
    sudo chmod 777 "${disk_path}"
    sudo rm -rf "${disk_path}"/*

    docker volume rm pulsarconf

    echo "Starting pulsar"
    docker run --rm -it --name pulsar -d --cpus $docker_cpus \
    -p 6650:6650 \
    -p 8080:8080 \
    --mount type=bind,src=$disk_path,dst=/pulsar/data \
    --mount source=pulsarconf,target=/pulsar/conf \
    apachepulsar/pulsar:2.5.0 \
    bin/pulsar standalone 
  
fi
  
echo "Running test"
time $pulsartest_bin --duration $duration --messages-per-second $messages_per_second --producer-batching true --where $location --storage $storage --pulsar-deployment-cpus $docker_cpus --topics $topics --pulsar-threads $pulsar_threads --asio-threads $asio_threads

if $restart_pulsar; then
    echo "Cleaning up..."
    docker stop pulsar
    sleep 10
fi
