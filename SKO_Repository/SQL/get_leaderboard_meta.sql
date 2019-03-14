DELIMITER $$
CREATE DEFINER=`nate`@`%` PROCEDURE `get_leaderboard_meta`()
BEGIN

SET SQL_SAFE_UPDATES = 0;

CREATE TEMPORARY TABLE results 
(
	PlayerCount INT DEFAULT 0, 
    OnlineCount INT DEFAULT 0
);
 
-- Find total player count.
INSERT INTO results (PlayerCount, OnlineCount)
SELECT
	COUNT(player.id) AS PlayerCount,
    0 AS OnlineCount
FROM sko.player AS player LIMIT 1;

-- Find players currently online.
UPDATE results
SET	OnlineCount = 
(
	SELECT 
		(s.playersWindows + s.playersLinux + s.playersMac) AS OnlineCount
	FROM `sko`.`status` AS s
	ORDER BY s.time DESC
	limit 1
);

-- Return results
SELECT 
	result.PlayerCount AS playerCount, 
    result.OnlineCount AS onlineCount 
FROM results result LIMIT 1;


END$$
DELIMITER ;
