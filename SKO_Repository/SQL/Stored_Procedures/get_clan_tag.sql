CREATE DEFINER="skoserver"@"%" PROCEDURE "get_clan_tag"(IN playerId int)
SELECT
	clan.name
FROM sko.player AS player
INNER JOIN sko.clan AS clan ON player.clan_id = clan.id

LIMIT 1