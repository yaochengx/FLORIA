






// the number of elements in the set accessing_one_set is <= number of ways
//
for each_address in accessing_one_set
    //wait until time elapse , visiting speed. shorter visit have a higher probability of hit. then we need to keep updating wait
    wait(time for that address)
    time=rtdsc()
    maccess(each_address)
    delta=rtdsc()-time
    if (delta<=threshold)
        hit++;
    else 
        miss++;
    occupied_way=hit
    return occupied_way;






