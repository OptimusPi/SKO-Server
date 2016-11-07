SET SQL_MODE="NO_AUTO_VALUE_ON_ZERO";
SET time_zone = "+00:00";

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;


CREATE TABLE IF NOT EXISTS `account` (
  `id` int(10) NOT NULL,
  `username` varchar(50) NOT NULL,
  `bio` varchar(255) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

CREATE TABLE IF NOT EXISTS `ban` (
  `idban` int(11) NOT NULL AUTO_INCREMENT,
  `player_id` int(11) NOT NULL,
  `banned_by` varchar(45) DEFAULT NULL,
  `ban_reason` varchar(45) DEFAULT NULL,
  PRIMARY KEY (`idban`),
  UNIQUE KEY `player_id_UNIQUE` (`player_id`),
  KEY `player_id` (`player_id`)
) ENGINE=MyISAM  DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

CREATE TABLE IF NOT EXISTS `bank` (
  `player_id` int(11) NOT NULL,
  `ITEM_0` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_1` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_2` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_3` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_4` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_5` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_6` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_7` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_8` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_9` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_10` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_11` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_12` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_13` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_14` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_15` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_16` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_17` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_18` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_19` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_20` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_21` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_22` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_23` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_24` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_25` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_26` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_27` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_28` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_29` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_30` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_31` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_32` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_33` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_34` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_35` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_36` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_37` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_38` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_39` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_40` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_41` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_42` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_43` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_44` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_45` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_46` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_47` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_48` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_49` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_50` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_51` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_52` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_53` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_54` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_55` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_56` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_57` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_58` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_59` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_60` int(11) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`player_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

CREATE TABLE IF NOT EXISTS `clan` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `owner_id` int(11) NOT NULL,
  `name` varchar(19) NOT NULL,
  `bio` varchar(255) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB  DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

CREATE TABLE IF NOT EXISTS `deleted_mail` (
  `mid` int(10) NOT NULL AUTO_INCREMENT,
  `from_id` int(11) NOT NULL,
  `to_id` int(11) NOT NULL,
  `sent_date` varchar(50) NOT NULL,
  `sent_time` varchar(50) NOT NULL,
  `subject` varchar(200) NOT NULL,
  `message` varchar(500) NOT NULL,
  `clan_req` int(10) NOT NULL DEFAULT '0',
  `opened` int(1) NOT NULL,
  `ip_addr` varchar(20) NOT NULL,
  PRIMARY KEY (`mid`)
) ENGINE=InnoDB  DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

CREATE TABLE IF NOT EXISTS `inventory` (
  `player_id` int(11) NOT NULL,
  `ITEM_0` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_1` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_2` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_3` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_4` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_5` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_6` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_7` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_8` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_9` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_10` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_11` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_12` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_13` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_14` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_15` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_16` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_17` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_18` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_19` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_20` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_21` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_22` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_23` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_24` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_25` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_26` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_27` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_28` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_29` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_30` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_31` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_32` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_33` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_34` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_35` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_36` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_37` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_38` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_39` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_40` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_41` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_42` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_43` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_44` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_45` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_46` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_47` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_48` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_49` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_50` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_51` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_52` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_53` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_54` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_55` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_56` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_57` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_58` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_59` int(11) unsigned NOT NULL DEFAULT '0',
  `ITEM_60` int(11) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`player_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

CREATE TABLE IF NOT EXISTS `ip_ban` (
  `ip` varchar(45) NOT NULL,
  `banned_by` varchar(45) DEFAULT NULL,
  `ban_reason` varchar(45) DEFAULT NULL,
  PRIMARY KEY (`ip`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

CREATE TABLE IF NOT EXISTS `ip_log` (
  `idip_log` int(11) NOT NULL AUTO_INCREMENT,
  `player_id` int(11) NOT NULL,
  `ip` varchar(45) NOT NULL,
  PRIMARY KEY (`player_id`,`ip`),
  UNIQUE KEY `idip_log` (`idip_log`)
) ENGINE=MyISAM  DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

CREATE TABLE IF NOT EXISTS `mail` (
  `mid` int(10) NOT NULL AUTO_INCREMENT,
  `from_id` int(11) NOT NULL,
  `to_id` int(11) NOT NULL,
  `sent_date` varchar(50) NOT NULL,
  `sent_time` varchar(50) NOT NULL,
  `subject` varchar(200) NOT NULL,
  `message` varchar(500) NOT NULL,
  `clan_req` int(10) NOT NULL DEFAULT '0',
  `opened` int(1) NOT NULL DEFAULT '0',
  `ip_addr` varchar(20) NOT NULL,
  PRIMARY KEY (`mid`)
) ENGINE=InnoDB  DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

CREATE TABLE IF NOT EXISTS `moderator` (
  `idmoderator` int(11) NOT NULL AUTO_INCREMENT,
  `player_id` int(11) NOT NULL,
  PRIMARY KEY (`idmoderator`),
  UNIQUE KEY `player_id_UNIQUE` (`player_id`)
) ENGINE=MyISAM  DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

CREATE TABLE IF NOT EXISTS `mute` (
  `player_id` int(11) NOT NULL,
  `muted_by` varchar(45) DEFAULT NULL,
  PRIMARY KEY (`player_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

CREATE TABLE IF NOT EXISTS `orders` (
  `id` int(10) NOT NULL AUTO_INCREMENT,
  `playerid` int(10) NOT NULL,
  `item` int(10) NOT NULL,
  `date` varchar(50) NOT NULL,
  `time` varchar(50) NOT NULL,
  `amount` varchar(50) NOT NULL,
  `method` varchar(50) NOT NULL,
  `transid` varchar(255) NOT NULL,
  `status` varchar(20) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `transid` (`transid`)
) ENGINE=InnoDB  DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

CREATE TABLE IF NOT EXISTS `player` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `username` varchar(45) NOT NULL,
  `password` varchar(45) NOT NULL,
  `level` int(11) unsigned NOT NULL,
  `x` float NOT NULL,
  `y` float NOT NULL,
  `xp` int(11) unsigned NOT NULL,
  `hp` int(11) unsigned NOT NULL,
  `str` int(11) unsigned NOT NULL,
  `def` int(11) unsigned NOT NULL,
  `xp_max` int(11) unsigned NOT NULL,
  `hp_max` int(11) unsigned NOT NULL,
  `y_speed` float NOT NULL,
  `x_speed` float NOT NULL,
  `stat_points` int(11) unsigned NOT NULL,
  `regen` int(11) unsigned NOT NULL,
  `facing_right` bit(1) NOT NULL,
  `EQUIP_0` int(11) unsigned NOT NULL,
  `EQUIP_1` int(11) unsigned NOT NULL,
  `EQUIP_2` int(11) unsigned NOT NULL,
  `VERSION_OS` tinyint(4) unsigned NOT NULL DEFAULT '0',
  `minutes_played` bigint(20) unsigned NOT NULL DEFAULT '0',
  `current_map` int(11) unsigned NOT NULL DEFAULT '0',
  `inventory_order` char(65) NOT NULL,
  `clan_id` int(11) NOT NULL,
  `email` varchar(255) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `username` (`username`)
) ENGINE=MyISAM  DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

CREATE TABLE IF NOT EXISTS `status` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `playersWindows` int(11) NOT NULL,
  `playersLinux` int(11) NOT NULL,
  `playersMac` int(11) NOT NULL,
  `averagePing` int(11) NOT NULL,
  UNIQUE KEY `id` (`id`)
) ENGINE=InnoDB  DEFAULT CHARSET=latin1 AUTO_INCREMENT=1 ;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
