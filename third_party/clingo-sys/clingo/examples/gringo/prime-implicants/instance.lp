model(1). true(1,1). true(1,2).                       % { 1, 2,~3,~4}
model(2). true(2,1).            true(2,3). true(2,4). % { 1,~2, 3, 4}
model(3). true(3,1).            true(3,3).            % { 1,~2, 3,~4}
model(4).                       true(4,3).            % {~1,~2, 3,~4}
model(5).                       true(5,3). true(5,4). % {~1,~2, 3, 4}
model(6).                                  true(6,4). % {~1,~2,~3, 4}
model(7).            true(7,2).            true(7,4). % {~1, 2,~3, 4}

% Prime implicants: {1,2,~3,~4}, {~2,3}, {~1,~2,4}, {~1,~3,4}
