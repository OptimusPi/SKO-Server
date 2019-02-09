DELIMITER $$
CREATE DEFINER=`nate`@`%` PROCEDURE `get_leaderboard_page`(IN pagenum INT, IN pagesize INT)
BEGIN
DECLARE startnum INT;
DECLARE endnum INT;

SET startnum = (pagenum-1) * pagesize;
SET endnum = pagesize;

SELECT
	player.id AS playerId,
    player.username AS playerName,
    player.level,
    player.xp,
    player.xp_max,
    player.str,
    player.def,
    player.regen,
    player.hp,
    player.hp_max,
    CASE WHEN player.VERSION_OS = 0 THEN 'Windows'
		 WHEN player.VERSION_OS = 1 THEN 'Linux'
         WHEN player.VERSION_OS = 2 THEN 'Mac OSX'
	END AS operatingSystem,
    player.minutes_played,
    inventory.ITEM_0 + bank.ITEM_0 AS gold,
    clan.name AS clanName
FROM sko.player AS player
LEFT JOIN 
	sko.clan AS clan 
ON clan.id = player.clan_id
LEFT JOIN 
	sko.inventory AS inventory
ON inventory.player_id = player.id
LEFT JOIN 
	sko.bank AS bank
ON bank.player_id = player.id

ORDER BY player.level DESC, 
         player.xp DESC, 
         player.minutes_played DESC, 
         player.id DESC
LIMIT startnum,endnum;

END$$
DELIMITER ;
